import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.json.JSONObject;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

import io.jsonwebtoken.*;
import io.jsonwebtoken.lang.Maps;

import java.nio.charset.StandardCharsets;
import java.security.KeyFactory;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.interfaces.RSAPrivateKey;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.Base64;
import java.util.Map;

public class JsonWebTokenFactory {
    //读取json文件为字符串
    public static String getValue(String name) throws IOException {
        //serviceAccountPrivateKey路径
        String content = new String(Files.readAllBytes(Paths.get("src\\main\\java\\serviceAccountPrivateKey.json")));
        JSONObject json = new JSONObject(content);
        return json.getString(name);
    }

    // 实际开发时请从服务账号密钥文件中读取private_key
    // 请在代码中加密私钥，仅获取 '-----BEGIN PRIVATE KEY-----\n' 和 '\n-----END PRIVATE KEY-----\n'之间的字符

    private static final String PRIVATE_KEY;

    static {
        try {
            PRIVATE_KEY = getValue("private_key");
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    // 实际开发时请从服务账号密钥文件中读取sub_account
    private static final String ISS;

    static {
        try {
            ISS = getValue("sub_account");
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    // 实际开发时请从服务账号密钥文件中读取key_id
    private static final String KID;

    static {
        try {
            KID = getValue("key_id");
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    // 实际开发时请将公网地址存储在配置文件或数据库
    private static final String AUD = "https://oauth-login.cloud.huawei.com/oauth2/v3/token";

    public static String createJwt() throws NoSuchAlgorithmException, InvalidKeySpecException, JsonProcessingException {
        RSAPrivateKey privateKey = (RSAPrivateKey) generatePrivateKey(PRIVATE_KEY);
        long iat = System.currentTimeMillis() / 1000;
        long exp = iat + 3600;

        Map<String, Object> header = Maps.<String, Object>of(JwsHeader.KEY_ID, KID)
                .and(JwsHeader.TYPE, JwsHeader.JWT_TYPE)
                .and(JwsHeader.ALGORITHM, SignatureAlgorithm.PS256.getValue())
                .build();

        Map<String, Object> payload = Maps.<String, Object>of(Claims.ISSUER, ISS)
                .and(Claims.ISSUED_AT, iat)
                .and(Claims.EXPIRATION, exp)
                .and(Claims.AUDIENCE, AUD)
                .build();

        return Jwts.builder()
                .setHeader(header)
                .setPayload(new ObjectMapper().writeValueAsString(payload))
                .signWith(privateKey, SignatureAlgorithm.PS256)
                .compact();
    }

    private static PrivateKey generatePrivateKey(String base64Key) throws NoSuchAlgorithmException, InvalidKeySpecException {
        PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(Base64.getDecoder().decode(base64Key.getBytes(StandardCharsets.UTF_8)));
        KeyFactory keyFactory = KeyFactory.getInstance("RSA");
        return keyFactory.generatePrivate(keySpec);
    }

    public static void main(String args[]) throws InvalidKeySpecException, NoSuchAlgorithmException, IOException {
        JsonWebTokenFactory jsonWebTokenFactory = new JsonWebTokenFactory();
        String Jwt = jsonWebTokenFactory.createJwt();// 获取鉴权令牌

        CloseableHttpClient httpClient = HttpClients.createDefault();

        /**
         * todo
         * 将project_id改为serviceAccountPrivateKey.json的project_id
         */
        String url = "https://push-api.cloud.huawei.com/v3/[project_id]/messages:send";
        HttpPost postRequest = new HttpPost(url);

        postRequest.setHeader("Content-Type", "application/json");
        postRequest.setHeader("Authorization", "Bearer " + Jwt);
        postRequest.setHeader("push-type","0");

        //messageBody路径
        String jsonPayload = new String(Files.readAllBytes(Paths.get("src\\main\\java\\messageBody.json")));
        postRequest.setEntity(new StringEntity(jsonPayload,"UTF-8"));

        CloseableHttpResponse response = httpClient.execute(postRequest);

        String responseString = EntityUtils.toString(response.getEntity(), "UTF-8");
        System.out.println("Response: " + responseString);

        response.close();
        httpClient.close();
    }
}
