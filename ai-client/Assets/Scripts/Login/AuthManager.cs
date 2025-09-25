using System;
using System.IO;
using UnityEngine;
using Utility;

public class AuthManager : Singleton<AuthManager>
{
    private static readonly string REFRESH_TOKEN_PATH = "/.user.refresh_token";
    
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

        RefreshToken = File.ReadAllText(_tokenPath);
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
            File.WriteAllText(_tokenPath, RefreshToken);
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
