import com.sun.net.httpserver.Headers;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;

import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.time.Instant;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

public class CarCloudServer {
    private static final int DEFAULT_PORT = 8080;
    private static final int HISTORY_LIMIT = 240;
    private static final String DEFAULT_DEVICE_ID = "6a9643457f2e6c302f94fcf9_qstcar";
    private static final String DEFAULT_ENDPOINT = "https://3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com";
    private static final String DEFAULT_PROJECT_ID = "01a05aee135276f59f135b5342b239bb";
    private static final String DEFAULT_INSTANCE_ID = "5357e9ef-bcdf-4934-9c94-2924c34fde2b";
    private static final String DEFAULT_SERVICE_ID = "qstcar";
    private static final String SIGN_ALGORITHM = "SDK-HMAC-SHA256";
    private static final String DERIVED_SIGN_ALGORITHM = "V11-HMAC-SHA256";
    private static final String DEFAULT_REGION_ID = "cn-north-4";
    private static final String DEFAULT_DERIVED_SERVICE_NAME = "iotda";
    private static final String DEFAULT_READ_MODE = "amqp";
    private static final String DEFAULT_AMQP_HOST = "3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com";
    private static final int DEFAULT_AMQP_PORT = 5671;
    private static final String DEFAULT_AMQP_QUEUE = "qst_queue";

    private final HttpClient client = HttpClient.newBuilder().build();
    private final List<Map<String, Object>> history = new ArrayList<>();
    private Map<String, Object> lastCloudSnapshot = null;
    private final Config config;
    private final AtomicBoolean amqpStarted = new AtomicBoolean(false);
    private volatile String amqpStatus = "not_started";
    private long amqpMessageCount = 0;
    private Instant lastAmqpMessageAt = null;
    private long lastAmqpMessageDeltaSeconds = -1;

    public CarCloudServer(Config config) {
        this.config = config;
    }

    public static void main(String[] args) throws Exception {
        Config config = Config.fromEnv(args);
        CarCloudServer app = new CarCloudServer(config);
        HttpServer server = HttpServer.create(new InetSocketAddress(config.port), 0);
        server.createContext("/", app::serveStatic);
        server.createContext("/api/config", app::serveConfig);
        server.createContext("/api/latest", app::serveLatest);
        server.createContext("/api/history", app::serveHistory);
        server.setExecutor(Executors.newCachedThreadPool());
        app.startBackgroundConsumers();
        server.start();

        System.out.println("Car realtime dashboard started.");
        System.out.println("Open: http://localhost:" + config.port + "/car.html");
        System.out.println("Huawei mode: " + (config.isCloudConfigured() ? "cloud/" + config.authMode() : "mock"));
        if (!config.isCloudConfigured()) {
            System.out.println("Set HUAWEICLOUD_SDK_AK and HUAWEICLOUD_SDK_SK to read live IoTDA shadow data.");
        }
    }

    private void serveStatic(HttpExchange exchange) throws IOException {
        if (handleCorsPreflight(exchange)) {
            return;
        }
        String path = exchange.getRequestURI().getPath();
        if (path.equals("/") || path.isBlank()) {
            path = "/car.html";
        }
        if (path.contains("..")) {
            send(exchange, 403, "text/plain; charset=utf-8", "Forbidden");
            return;
        }

        Path file = Path.of(".").resolve(path.substring(1)).normalize();
        if (!Files.exists(file) || Files.isDirectory(file)) {
            send(exchange, 404, "text/plain; charset=utf-8", "Not found");
            return;
        }

        String contentType = path.endsWith(".html") ? "text/html; charset=utf-8" : "application/octet-stream";
        send(exchange, 200, contentType, Files.readAllBytes(file));
    }

    private void serveLatest(HttpExchange exchange) throws IOException {
        if (handleCorsPreflight(exchange)) {
            return;
        }
        if (!"GET".equalsIgnoreCase(exchange.getRequestMethod())) {
            send(exchange, 405, "application/json; charset=utf-8", "{\"error\":\"method_not_allowed\"}");
            return;
        }

        Map<String, Object> data;
        try {
            data = config.isCloudConfigured() ? readHuaweiCloud() : mockSnapshot();
        } catch (Exception ex) {
            data = cloudErrorSnapshot(ex);
        }
        addHistory(data);
        send(exchange, 200, "application/json; charset=utf-8", toJson(data));
    }

    private void serveHistory(HttpExchange exchange) throws IOException {
        if (handleCorsPreflight(exchange)) {
            return;
        }
        synchronized (history) {
            send(exchange, 200, "application/json; charset=utf-8", toJson(history));
        }
    }

    private void serveConfig(HttpExchange exchange) throws IOException {
        if (handleCorsPreflight(exchange)) {
            return;
        }
        Map<String, Object> data = new LinkedHashMap<>();
        data.put("device_id", config.deviceId);
        data.put("service_id", config.serviceId);
        data.put("endpoint", config.endpoint);
        data.put("instance_id", config.instanceId);
        data.put("read_mode", config.readMode);
        data.put("amqp_host", config.amqpHost);
        data.put("amqp_port", config.amqpPort);
        data.put("amqp_queue", config.amqpQueue);
        data.put("amqp_status", appSafeStatus());
        synchronized (this) {
            data.put("amqp_message_count", amqpMessageCount);
            data.put("amqp_message_delta_seconds", lastAmqpMessageDeltaSeconds);
            data.put("amqp_last_message_at", lastAmqpMessageAt == null ? "" : lastAmqpMessageAt.toString());
        }
        data.put("cloud_configured", config.isCloudConfigured());
        data.put("mode", config.isCloudConfigured() ? "cloud/" + config.authMode() : "mock");
        send(exchange, 200, "application/json; charset=utf-8", toJson(data));
    }

    private String appSafeStatus() {
        return config.readMode.equalsIgnoreCase("amqp") ? amqpStatus : "disabled";
    }

    private void startBackgroundConsumers() {
        if (config.readMode.equalsIgnoreCase("amqp")) {
            startAmqpReceiver();
        }
    }

    private static boolean handleCorsPreflight(HttpExchange exchange) throws IOException {
        Headers headers = exchange.getResponseHeaders();
        headers.set("Access-Control-Allow-Origin", "*");
        headers.set("Access-Control-Allow-Methods", "GET, OPTIONS");
        headers.set("Access-Control-Allow-Headers", "Content-Type");
        if ("OPTIONS".equalsIgnoreCase(exchange.getRequestMethod())) {
            exchange.sendResponseHeaders(204, -1);
            exchange.close();
            return true;
        }
        return false;
    }

    private static void signGetRequest(HttpRequest.Builder requestBuilder, URI uri, String accessKey, String secretKey) {
        String xSdkDate = DateTimeFormatter.ofPattern("yyyyMMdd'T'HHmmss'Z'")
                .withZone(ZoneOffset.UTC)
                .format(Instant.now());
        String host = uri.getHost();
        String canonicalUri = canonicalUri(uri.getRawPath());
        String canonicalQuery = uri.getRawQuery() == null ? "" : uri.getRawQuery();
        String signedHeaders = "host;x-sdk-date";
        String canonicalHeaders = "host:" + host + "\n" + "x-sdk-date:" + xSdkDate + "\n";
        String payloadHash = sha256Hex("");
        String canonicalRequest = "GET\n" + canonicalUri + "\n" + canonicalQuery + "\n" +
                canonicalHeaders + "\n" + signedHeaders + "\n" + payloadHash;
        String stringToSign = SIGN_ALGORITHM + "\n" + xSdkDate + "\n" + sha256Hex(canonicalRequest);
        String signature = hmacSha256Hex(secretKey, stringToSign);
        String authorization = SIGN_ALGORITHM + " Access=" + accessKey +
                ", SignedHeaders=" + signedHeaders + ", Signature=" + signature;

        requestBuilder.header("X-Sdk-Date", xSdkDate);
        requestBuilder.header("Authorization", authorization);
    }

    private static void signDerivedGetRequest(HttpRequest.Builder requestBuilder, URI uri, Config config) {
        String xSdkDate = DateTimeFormatter.ofPattern("yyyyMMdd'T'HHmmss'Z'")
                .withZone(ZoneOffset.UTC)
                .format(Instant.now());
        String shortDate = xSdkDate.substring(0, 8);
        String host = uri.getHost();
        String canonicalUri = canonicalUri(uri.getRawPath());
        String canonicalQuery = uri.getRawQuery() == null ? "" : uri.getRawQuery();
        String signedHeaders = config.instanceId.isBlank()
                ? "accept;host;x-project-id;x-sdk-date"
                : "accept;host;instance-id;x-project-id;x-sdk-date";
        String canonicalHeaders = "accept:application/json\n" +
                "host:" + host + "\n" +
                (config.instanceId.isBlank() ? "" : "instance-id:" + config.instanceId + "\n") +
                "x-project-id:" + config.projectId + "\n" +
                "x-sdk-date:" + xSdkDate + "\n";
        String payloadHash = sha256Hex("");
        String canonicalRequest = "GET\n" + canonicalUri + "\n" + canonicalQuery + "\n" +
                canonicalHeaders + "\n" + signedHeaders + "\n" + payloadHash;
        String credentialInfo = shortDate + "/" + config.regionId + "/" + config.derivedServiceName;
        String stringToSign = DERIVED_SIGN_ALGORITHM + "\n" + xSdkDate + "\n" +
                credentialInfo + "\n" + sha256Hex(canonicalRequest);
        String derivedSigningKey = deriveSigningKey(config.accessKey, config.secretKey, credentialInfo);
        String signature = hmacSha256Hex(derivedSigningKey, stringToSign);
        String authorization = DERIVED_SIGN_ALGORITHM + " Credential=" + config.accessKey + "/" + credentialInfo +
                ", SignedHeaders=" + signedHeaders + ", Signature=" + signature;

        requestBuilder.header("X-Project-Id", config.projectId);
        requestBuilder.header("X-Sdk-Date", xSdkDate);
        requestBuilder.header("Authorization", authorization);
    }

    private static String deriveSigningKey(String accessKey, String secretKey, String info) {
        byte[] prk = hmacSha256(accessKey.getBytes(StandardCharsets.UTF_8), secretKey.getBytes(StandardCharsets.UTF_8));
        byte[] input = new byte[info.getBytes(StandardCharsets.UTF_8).length + 1];
        byte[] infoBytes = info.getBytes(StandardCharsets.UTF_8);
        System.arraycopy(infoBytes, 0, input, 0, infoBytes.length);
        input[input.length - 1] = 1;
        return hex(hmacSha256(prk, input));
    }

    private static byte[] hmacSha256(byte[] key, byte[] value) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(key, "HmacSHA256"));
            return mac.doFinal(value);
        } catch (Exception ex) {
            throw new IllegalStateException(ex);
        }
    }

    private static String canonicalUri(String rawPath) {
        String path = rawPath == null || rawPath.isBlank() ? "/" : rawPath;
        return path.endsWith("/") ? path : path + "/";
    }

    private static String sha256Hex(String value) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            return hex(digest.digest(value.getBytes(StandardCharsets.UTF_8)));
        } catch (Exception ex) {
            throw new IllegalStateException(ex);
        }
    }

    private static String hmacSha256Hex(String secret, String value) {
        try {
            Mac mac = Mac.getInstance("HmacSHA256");
            mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
            return hex(mac.doFinal(value.getBytes(StandardCharsets.UTF_8)));
        } catch (Exception ex) {
            throw new IllegalStateException(ex);
        }
    }

    private static String hex(byte[] bytes) {
        char[] digits = "0123456789abcdef".toCharArray();
        char[] chars = new char[bytes.length * 2];
        for (int i = 0; i < bytes.length; i++) {
            int value = bytes[i] & 0xff;
            chars[i * 2] = digits[value >>> 4];
            chars[i * 2 + 1] = digits[value & 0x0f];
        }
        return new String(chars);
    }

    private Map<String, Object> readHuaweiCloud() throws IOException, InterruptedException {
        if (config.readMode.equalsIgnoreCase("amqp")) {
            return readAmqpSnapshot();
        }
        if (config.readMode.equalsIgnoreCase("properties") || config.readMode.equalsIgnoreCase("realtime")) {
            return readHuaweiProperties();
        }
        return readHuaweiShadow();
    }

    private Map<String, Object> readAmqpSnapshot() throws IOException {
        startAmqpReceiver();
        synchronized (this) {
            if (lastCloudSnapshot == null) {
                throw new IOException("AMQP waiting for messages, status=" + amqpStatus);
            }
            Map<String, Object> snapshot = new LinkedHashMap<>(lastCloudSnapshot);
            snapshot.put("time", Instant.now().toString());
            snapshot.put("read_mode", "amqp");
            enrichCloudSnapshot(snapshot);
            return snapshot;
        }
    }

    private Map<String, Object> readHuaweiShadow() throws IOException, InterruptedException {
        String deviceId = URLEncoder.encode(config.deviceId, StandardCharsets.UTF_8);
        String url = trimRight(config.endpoint, "/") + "/v5/iot/" + config.projectId + "/devices/" + deviceId + "/shadow";
        URI uri = URI.create(url);
        HttpRequest.Builder requestBuilder = HttpRequest.newBuilder(uri)
                .GET()
                .header("Accept", "application/json");
        if (!config.instanceId.isBlank()) {
            requestBuilder.header("Instance-Id", config.instanceId);
        }
        if (!config.iamToken.isBlank()) {
            requestBuilder.header("X-Auth-Token", config.iamToken);
        } else {
            if (config.useDerivedAuth()) {
                signDerivedGetRequest(requestBuilder, uri, config);
            } else {
                signGetRequest(requestBuilder, uri, config.accessKey, config.secretKey);
            }
        }
        HttpRequest request = requestBuilder.build();

        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("HTTP " + response.statusCode() + " " + response.body());
        }

        Map<String, Object> snapshot = extractServiceProperties(response.body(), config.serviceId);
        snapshot.put("time", Instant.now().toString());
        snapshot.put("device_id", config.deviceId);
        snapshot.put("read_mode", "shadow");
        enrichCloudSnapshot(snapshot);
        saveLastCloudSnapshot(snapshot);
        return snapshot;
    }

    private Map<String, Object> readHuaweiProperties() throws IOException, InterruptedException {
        String deviceId = URLEncoder.encode(config.deviceId, StandardCharsets.UTF_8);
        String serviceId = URLEncoder.encode(config.serviceId, StandardCharsets.UTF_8);
        String url = trimRight(config.endpoint, "/") + "/v5/iot/" + config.projectId + "/devices/" + deviceId +
                "/properties?service_id=" + serviceId;
        URI uri = URI.create(url);
        HttpRequest.Builder requestBuilder = HttpRequest.newBuilder(uri)
                .GET()
                .header("Accept", "application/json");
        if (!config.instanceId.isBlank()) {
            requestBuilder.header("Instance-Id", config.instanceId);
        }
        if (!config.iamToken.isBlank()) {
            requestBuilder.header("X-Auth-Token", config.iamToken);
        } else if (config.useDerivedAuth()) {
            signDerivedGetRequest(requestBuilder, uri, config);
        } else {
            signGetRequest(requestBuilder, uri, config.accessKey, config.secretKey);
        }

        HttpResponse<String> response = client.send(requestBuilder.build(), HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        if (response.statusCode() < 200 || response.statusCode() >= 300) {
            throw new IOException("HTTP " + response.statusCode() + " " + response.body());
        }
        Map<String, Object> snapshot = extractRealtimeProperties(response.body(), config.serviceId);
        snapshot.put("time", Instant.now().toString());
        snapshot.put("device_id", config.deviceId);
        snapshot.put("read_mode", "properties");
        enrichCloudSnapshot(snapshot);
        saveLastCloudSnapshot(snapshot);
        return snapshot;
    }

    private static void enrichCloudSnapshot(Map<String, Object> snapshot) {
        snapshot.put("mock", false);
        Long eventAgeSeconds = cloudEventAgeSeconds(snapshot.get("event_time"));
        if (eventAgeSeconds != null) {
            snapshot.put("cloud_event_age_seconds", eventAgeSeconds);
            snapshot.put("stale", eventAgeSeconds > 15);
        } else {
            snapshot.put("stale", false);
        }
    }

    private static Long cloudEventAgeSeconds(Object value) {
        if (!(value instanceof String text)) {
            return null;
        }
        try {
            Instant eventTime = DateTimeFormatter.ofPattern("yyyyMMdd'T'HHmmss'Z'")
                    .withZone(ZoneOffset.UTC)
                    .parse(text, Instant::from);
            return Math.max(0, Instant.now().getEpochSecond() - eventTime.getEpochSecond());
        } catch (Exception ex) {
            return null;
        }
    }

    private Map<String, Object> cloudErrorSnapshot(Exception ex) {
        Map<String, Object> data;
        synchronized (this) {
            data = lastCloudSnapshot == null ? new LinkedHashMap<>() : new LinkedHashMap<>(lastCloudSnapshot);
        }
        data.put("time", Instant.now().toString());
        data.put("device_id", config.deviceId);
        data.put("mock", false);
        data.put("stale", true);
        data.put("error", "cloud_fetch_failed: " + ex.getMessage());
        return data;
    }

    private synchronized void saveLastCloudSnapshot(Map<String, Object> snapshot) {
        lastCloudSnapshot = new LinkedHashMap<>(snapshot);
    }

    private synchronized void saveAmqpSnapshot(Map<String, Object> snapshot) {
        Instant now = Instant.now();
        if (lastAmqpMessageAt != null) {
            lastAmqpMessageDeltaSeconds = Math.max(0, now.getEpochSecond() - lastAmqpMessageAt.getEpochSecond());
        }
        lastAmqpMessageAt = now;
        amqpMessageCount++;
        snapshot.put("amqp_message_count", amqpMessageCount);
        snapshot.put("amqp_message_delta_seconds", lastAmqpMessageDeltaSeconds);
        snapshot.put("amqp_last_message_at", lastAmqpMessageAt.toString());
        lastCloudSnapshot = new LinkedHashMap<>(snapshot);
    }

    private void startAmqpReceiver() {
        if (!amqpStarted.compareAndSet(false, true)) {
            return;
        }
        Thread thread = new Thread(this::runAmqpReceiver, "HuaweiAmqpReceiver");
        thread.setDaemon(true);
        thread.start();
    }

    private void runAmqpReceiver() {
        while (true) {
            Object connection = null;
            try {
                ensureAmqpConfig();
                amqpStatus = "connecting";
                Class<?> factoryClass = Class.forName("org.apache.qpid.jms.JmsConnectionFactory");
                Object factory = factoryClass.getConstructor(String.class).newInstance(config.amqpUri());
                connection = factoryClass.getMethod("createConnection", String.class, String.class)
                        .invoke(factory, config.amqpUsername(), config.amqpAccessCode);
                connection.getClass().getMethod("start").invoke(connection);
                Object session = connection.getClass().getMethod("createSession", boolean.class, int.class)
                        .invoke(connection, false, 1);
                Class<?> destinationClass = jmsClass("Destination");
                Object queue = createJmsQueue(session, config.amqpQueue);
                Object consumer = session.getClass().getMethod("createConsumer", destinationClass).invoke(session, queue);
                amqpStatus = "connected";

                while (true) {
                    Object message = consumer.getClass().getMethod("receive", long.class).invoke(consumer, 1000L);
                    if (message == null) {
                        continue;
                    }
                    String text = readJmsMessageText(message);
                    if (text == null || text.isBlank()) {
                        continue;
                    }
                    Map<String, Object> snapshot = extractServiceProperties(text, config.serviceId);
                    snapshot.put("device_id", stringField(text, "device_id", config.deviceId));
                    snapshot.put("event_time", stringField(text, "event_time", String.valueOf(snapshot.getOrDefault("event_time", ""))));
                    snapshot.put("raw_message_time", Instant.now().toString());
                    snapshot.put("read_mode", "amqp");
                    enrichCloudSnapshot(snapshot);
                    saveAmqpSnapshot(snapshot);
                    amqpStatus = "message_received";
                }
            } catch (Exception ex) {
                amqpStatus = "error: " + rootMessage(ex);
                closeReflective(connection);
                sleepQuietly(3000);
            }
        }
    }

    private void ensureAmqpConfig() throws IOException {
        if (config.amqpAccessKey.isBlank() || config.amqpAccessCode.isBlank()) {
            throw new IOException("AMQP access key/code is not configured");
        }
    }

    private static String readJmsMessageText(Object message) {
        try {
            Object value = message.getClass().getMethod("getBody", Class.class).invoke(message, String.class);
            return value == null ? "" : String.valueOf(value);
        } catch (Exception ignored) {
            try {
                Object value = message.getClass().getMethod("getText").invoke(message);
                return value == null ? "" : String.valueOf(value);
            } catch (Exception ignoredAgain) {
                return "";
            }
        }
    }

    private static Class<?> jmsClass(String simpleName) throws ClassNotFoundException {
        try {
            return Class.forName("jakarta.jms." + simpleName);
        } catch (ClassNotFoundException ignored) {
            return Class.forName("javax.jms." + simpleName);
        }
    }

    private static Object createJmsQueue(Object session, String queueName) throws Exception {
        try {
            Class<?> queueClass = Class.forName("org.apache.qpid.jms.JmsQueue");
            return queueClass.getConstructor(String.class).newInstance(queueName);
        } catch (ClassNotFoundException ignored) {
            return session.getClass().getMethod("createQueue", String.class).invoke(session, queueName);
        }
    }

    private static String stringField(String body, String key, String fallback) {
        Pattern pattern = Pattern.compile("\"" + Pattern.quote(key) + "\"\\s*:\\s*\"([^\"]*)\"");
        Matcher matcher = pattern.matcher(body);
        return matcher.find() ? matcher.group(1) : fallback;
    }

    private static String rootMessage(Exception ex) {
        Throwable item = ex;
        while (item.getCause() != null) {
            item = item.getCause();
        }
        return item.getMessage() == null ? item.getClass().getSimpleName() : item.getMessage();
    }

    private static void closeReflective(Object closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.getClass().getMethod("close").invoke(closeable);
        } catch (Exception ignored) {
        }
    }

    private static void sleepQuietly(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException ex) {
            Thread.currentThread().interrupt();
        }
    }

    private static Map<String, Object> extractServiceProperties(String body, String serviceId) throws IOException {
        Pattern servicePattern = Pattern.compile("\"service_id\"\\s*:\\s*\"" + Pattern.quote(serviceId) + "\"");
        Matcher serviceMatcher = servicePattern.matcher(body);
        if (!serviceMatcher.find()) {
            throw new IOException("service_id not found: " + serviceId);
        }
        int servicePos = serviceMatcher.start();

        int propertiesKey = body.indexOf("\"properties\"", servicePos);
        if (propertiesKey < 0) {
            throw new IOException("properties not found for service: " + serviceId);
        }
        int objectStart = body.indexOf('{', propertiesKey);
        int objectEnd = findMatchingBrace(body, objectStart);
        if (objectStart < 0 || objectEnd <= objectStart) {
            throw new IOException("invalid properties object");
        }

        return parseFlatJsonObject(body.substring(objectStart + 1, objectEnd));
    }

    private static Map<String, Object> extractRealtimeProperties(String body, String serviceId) throws IOException {
        Pattern servicePattern = Pattern.compile("\"service_id\"\\s*:\\s*\"" + Pattern.quote(serviceId) + "\"");
        Matcher serviceMatcher = servicePattern.matcher(body);
        if (!serviceMatcher.find()) {
            int directProperties = body.indexOf("\"properties\"");
            if (directProperties >= 0) {
                return extractPropertiesObject(body, directProperties, "properties");
            }
            throw new IOException("service_id not found: " + serviceId + ", body=" + body);
        }

        int propertiesKey = body.indexOf("\"properties\"", serviceMatcher.start());
        if (propertiesKey < 0) {
            throw new IOException("properties not found for service: " + serviceId + ", body=" + body);
        }
        return extractPropertiesObject(body, propertiesKey, "properties");
    }

    private static Map<String, Object> extractPropertiesObject(String body, int propertiesKey, String label) throws IOException {
        int objectStart = body.indexOf('{', propertiesKey);
        int objectEnd = findMatchingBrace(body, objectStart);
        if (objectStart < 0 || objectEnd <= objectStart) {
            throw new IOException("invalid " + label + " object");
        }
        return parseFlatJsonObject(body.substring(objectStart + 1, objectEnd));
    }

    private static Map<String, Object> parseFlatJsonObject(String json) {
        Map<String, Object> result = new LinkedHashMap<>();
        Pattern pairPattern = Pattern.compile("\"([^\"]+)\"\\s*:\\s*(\"(?:\\\\.|[^\"])*\"|-?\\d+(?:\\.\\d+)?|true|false|null)");
        Matcher matcher = pairPattern.matcher(json);
        while (matcher.find()) {
            String key = matcher.group(1);
            String value = matcher.group(2);
            result.put(key, parseJsonValue(value));
        }
        return result;
    }

    private static Object parseJsonValue(String value) {
        if (value == null || value.equals("null")) {
            return null;
        }
        if (value.equals("true") || value.equals("false")) {
            return Boolean.parseBoolean(value);
        }
        if (value.startsWith("\"")) {
            return value.substring(1, value.length() - 1).replace("\\\"", "\"").replace("\\\\", "\\");
        }
        try {
            return value.contains(".") ? Double.parseDouble(value) : Long.parseLong(value);
        } catch (NumberFormatException ex) {
            return value;
        }
    }

    private static int findMatchingBrace(String text, int start) {
        if (start < 0) {
            return -1;
        }
        int depth = 0;
        boolean inString = false;
        boolean escaped = false;
        for (int i = start; i < text.length(); i++) {
            char c = text.charAt(i);
            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    return i;
                }
            }
        }
        return -1;
    }

    private Map<String, Object> mockSnapshot() {
        long now = System.currentTimeMillis();
        double wave = Math.sin(now / 9000.0);
        Map<String, Object> data = new LinkedHashMap<>();
        data.put("time", Instant.now().toString());
        data.put("device_id", config.deviceId);
        data.put("temp", round1(28.0 + wave * 0.8));
        data.put("humi", round1(47.0 + Math.cos(now / 11000.0) * 2.2));
        data.put("lumi", Math.round(330 + wave * 45));
        data.put("mode_led", "SEN");
        data.put("car_mode", "AUTO");
        data.put("temperature", round1(28.5 + wave * 0.8));
        data.put("humidity", round1(48.0 + Math.cos(now / 11000.0) * 2.2));
        data.put("ap_ir", Math.round(9 + wave * 4));
        data.put("ap_als", Math.round(334 + wave * 45));
        data.put("ap_ps", Math.round(137 + Math.cos(now / 8000.0) * 18));
        data.put("edge_left", Math.sin(now / 21000.0) > 0.94 ? 1 : 0);
        data.put("edge_right", Math.cos(now / 23000.0) > 0.94 ? 1 : 0);
        data.put("distance_cm", round1(28.7 + Math.sin(now / 7000.0) * 8.0));
        data.put("led_on", wave < -0.35 ? 1 : 0);
        data.put("mock", true);
        return data;
    }

    private void addHistory(Map<String, Object> data) {
        synchronized (history) {
            history.add(new LinkedHashMap<>(data));
            while (history.size() > HISTORY_LIMIT) {
                history.remove(0);
            }
        }
    }

    private static double round1(double value) {
        return Math.round(value * 10.0) / 10.0;
    }

    private static String trimRight(String value, String suffix) {
        while (value.endsWith(suffix)) {
            value = value.substring(0, value.length() - suffix.length());
        }
        return value;
    }

    private static void send(HttpExchange exchange, int status, String contentType, String body) throws IOException {
        send(exchange, status, contentType, body.getBytes(StandardCharsets.UTF_8));
    }

    private static void send(HttpExchange exchange, int status, String contentType, byte[] body) throws IOException {
        Headers headers = exchange.getResponseHeaders();
        headers.set("Content-Type", contentType);
        headers.set("Cache-Control", "no-store");
        headers.set("Access-Control-Allow-Origin", "*");
        exchange.sendResponseHeaders(status, body.length);
        try (OutputStream output = exchange.getResponseBody()) {
            output.write(body);
        }
    }

    private static String toJson(Object value) {
        if (value == null) {
            return "null";
        }
        if (value instanceof String s) {
            return "\"" + escapeJson(s) + "\"";
        }
        if (value instanceof Number || value instanceof Boolean) {
            return String.valueOf(value);
        }
        if (value instanceof Map<?, ?> map) {
            StringBuilder builder = new StringBuilder("{");
            boolean first = true;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                if (!first) {
                    builder.append(',');
                }
                first = false;
                builder.append(toJson(String.valueOf(entry.getKey()))).append(':').append(toJson(entry.getValue()));
            }
            return builder.append('}').toString();
        }
        if (value instanceof Iterable<?> items) {
            StringBuilder builder = new StringBuilder("[");
            boolean first = true;
            for (Object item : items) {
                if (!first) {
                    builder.append(',');
                }
                first = false;
                builder.append(toJson(item));
            }
            return builder.append(']').toString();
        }
        return toJson(String.valueOf(value));
    }

    private static String escapeJson(String value) {
        return value == null ? "" : value.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "\\r");
    }

    private record Config(int port, String endpoint, String projectId, String instanceId, String iamToken,
                          String accessKey, String secretKey, String regionId, String derivedServiceName,
                          String authType, String readMode, String deviceId, String serviceId,
                          String amqpHost, int amqpPort, String amqpQueue,
                          String amqpAccessKey, String amqpAccessCode) {
        static Config fromEnv(String[] args) {
            int port = args.length > 0 ? Integer.parseInt(args[0]) : intEnv("CAR_SERVER_PORT", DEFAULT_PORT);
            return new Config(
                    port,
                    env("HUAWEI_IOTDA_ENDPOINT", DEFAULT_ENDPOINT),
                    env("HUAWEI_PROJECT_ID", DEFAULT_PROJECT_ID),
                    env("HUAWEI_INSTANCE_ID", DEFAULT_INSTANCE_ID),
                    env("HUAWEI_IAM_TOKEN", ""),
                    firstEnv("HUAWEICLOUD_SDK_AK", "HUAWEI_ACCESS_KEY_ID"),
                    firstEnv("HUAWEICLOUD_SDK_SK", "HUAWEI_SECRET_ACCESS_KEY"),
                    env("HUAWEI_REGION_ID", DEFAULT_REGION_ID),
                    env("HUAWEI_DERIVED_SERVICE_NAME", DEFAULT_DERIVED_SERVICE_NAME),
                    env("HUAWEI_AUTH_TYPE", "derived"),
                    env("HUAWEI_READ_MODE", DEFAULT_READ_MODE),
                    env("HUAWEI_DEVICE_ID", DEFAULT_DEVICE_ID),
                    env("HUAWEI_SERVICE_ID", DEFAULT_SERVICE_ID),
                    env("HUAWEI_AMQP_HOST", DEFAULT_AMQP_HOST),
                    intEnv("HUAWEI_AMQP_PORT", DEFAULT_AMQP_PORT),
                    env("HUAWEI_AMQP_QUEUE", DEFAULT_AMQP_QUEUE),
                    firstNonBlank(firstEnv("HUAWEI_AMQP_ACCESS_KEY", "HUAWEI_AMQP_AK"),
                            firstEnv("HUAWEICLOUD_SDK_AK", "HUAWEI_ACCESS_KEY_ID")),
                    firstNonBlank(firstEnv("HUAWEI_AMQP_ACCESS_CODE", "HUAWEI_AMQP_SK"),
                            firstEnv("HUAWEICLOUD_SDK_SK", "HUAWEI_SECRET_ACCESS_KEY"))
            );
        }

        boolean isCloudConfigured() {
            if (readMode.equalsIgnoreCase("amqp")) {
                return !instanceId.isBlank() && !amqpAccessKey.isBlank() && !amqpAccessCode.isBlank();
            }
            return !projectId.isBlank() && (!iamToken.isBlank() || (!accessKey.isBlank() && !secretKey.isBlank()));
        }

        String authMode() {
            if (!iamToken.isBlank()) {
                return "token";
            }
            return useDerivedAuth() ? "aksk-derived" : "aksk";
        }

        boolean useDerivedAuth() {
            return !authType.equalsIgnoreCase("normal") && !authType.equalsIgnoreCase("basic");
        }

        String amqpUri() {
            return "failover:(amqps://" + amqpHost + ":" + amqpPort +
                    "?amqp.vhost=default&amqp.idleTimeout=30000&amqp.saslMechanisms=PLAIN)" +
                    "?jms.prefetchPolicy.queuePrefetch=1000&jms.clientID=qstcar-dashboard-" +
                    System.currentTimeMillis() + "&failover.reconnectDelay=3000&failover.maxReconnectDelay=30000";
        }

        String amqpUsername() {
            return "accessKey=" + amqpAccessKey + "|timestamp=" + System.currentTimeMillis() +
                    "|instanceId=" + instanceId;
        }

        private static String env(String name, String fallback) {
            String value = System.getenv(name);
            if (value == null || value.isBlank()) {
                return fallback;
            }
            value = value.trim();
            return value.equalsIgnoreCase("none") ? "" : value;
        }

        private static String firstEnv(String first, String second) {
            String firstValue = env(first, "");
            return firstValue.isBlank() ? env(second, "") : firstValue;
        }

        private static String firstNonBlank(String first, String second) {
            return first == null || first.isBlank() ? second : first;
        }

        private static int intEnv(String name, int fallback) {
            String value = System.getenv(name);
            if (value == null || value.isBlank()) {
                return fallback;
            }
            try {
                return Integer.parseInt(value.trim());
            } catch (NumberFormatException ex) {
                return fallback;
            }
        }
    }
}
