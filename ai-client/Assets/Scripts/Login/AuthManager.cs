using System;
using System.IO;
using Newtonsoft.Json;
using UnityEngine;
using Utility;

public class AuthManager : Singleton<AuthManager>
{
    [Serializable]
    public class TokenInfo
    {
        public string refreshToken;
        public string userName;
    }
    
    private static readonly string REFRESH_TOKEN_PATH = "/.user.refresh_info";
    
    public string RefreshToken { get; private set; }
    public string AccessToken { get; private set; }

    public Guid UserGuid { get; private set; } // Never changed
    public string Username { get; private set; }

    private string _tokenPath;

    public bool HasRefreshToken => !string.IsNullOrEmpty(RefreshToken);

    private void Awake()
    {
        _tokenPath = Application.persistentDataPath + REFRESH_TOKEN_PATH;

        LoadRefreshToken();
    }

    private void LoadRefreshToken()
    {
        if (!File.Exists(_tokenPath))
        {
            RefreshToken = string.Empty;
            return;
        }

        string loadedJson = File.ReadAllText(_tokenPath);
        var deserializedData = JsonConvert.DeserializeObject<TokenInfo>(loadedJson);
        RefreshToken = deserializedData.refreshToken;
        Username = deserializedData.userName;
    }

    public void InitTokens(LoginResponse response)
    {
        RefreshToken = response.refreshToken;
        AccessToken = response.accessToken;
        UserGuid = Guid.Parse(response.user.uid);
        Username = response.user.username;
        
        UpdateRefreshToken();
    }

    public void UpdateAccessToken(LoginResponse response)
    {
        AccessToken = response.accessToken;
        UserGuid = Guid.Parse(response.user.uid);
        Username = response.user.username;
    }
    
    private void UpdateRefreshToken()
    {
        try
        {
            if (string.IsNullOrEmpty(RefreshToken))
            {
                throw new ArgumentNullException($"Empty Refresh Token");
            }
            
            // Encrypt Refresh Token needed 
            var tokenInfo = new TokenInfo
            {
                refreshToken = RefreshToken,
                userName = Username
            };
            
            string serializeData = JsonConvert.SerializeObject(tokenInfo);
            File.WriteAllText(_tokenPath, serializeData);
        }
        catch (Exception ex)
        {
            Debug.LogError($"failed to Save Token: {ex.Message}");
        }
    }
    
    public void EraseAuthInformation()
    {
        RefreshToken = string.Empty;
        AccessToken = string.Empty;
        UserGuid = Guid.Empty;

        if (File.Exists(_tokenPath))
            File.Delete(_tokenPath); // 가지고 있는 토큰 정보도 삭제
    }
}
