using System;
using System.Collections;
using System.IO;
using Newtonsoft.Json;
using UnityEngine;
using Utility;

/// <summary>
/// This Class is global class for user Authentication by web server <br/>
/// Singleton Class <br/>
/// Refresh Token, Access Token, User Simple Info Include
/// </summary>
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
    public bool IsLoggedIn { get; private set; }

    // User Info Caching
    public Guid UserGuid { get; private set; }

    public string Username { get; private set; }

    private string _tokenPath;

    public bool HasRefreshToken => !string.IsNullOrEmpty(RefreshToken);

    private void Awake()
    {
        _tokenPath = Application.persistentDataPath + REFRESH_TOKEN_PATH;
        StartCoroutine(LoadRefreshToken());
    }

    private IEnumerator LoadRefreshToken()
    {
        if (!File.Exists(_tokenPath))
        {
            RefreshToken = string.Empty;
            yield break;
        }

        string loadedJson = File.ReadAllText(_tokenPath);
        var deserializedData = JsonConvert.DeserializeObject<TokenInfo>(loadedJson);
        RefreshToken = deserializedData.refreshToken;
        Username = deserializedData.userName;
    }

    public void InitTokens(LoginResponse response)
    {
        RefreshToken = response.RefreshToken;
        AccessToken = response.AccessToken;
        UserGuid = Guid.Parse(response.User.Uid);
        Username = response.User.Username;

        StartCoroutine(UpdateRefreshToken());
    }

    public void CompleteLogin()
    {
        IsLoggedIn = true;
    }

    public void UpdateAccessToken(LoginResponse response)
    {
        AccessToken = response.AccessToken;
        UserGuid = Guid.Parse(response.User.Uid);
        Username = response.User.Username;
    }

    private IEnumerator UpdateRefreshToken()
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
        yield return File.WriteAllTextAsync(_tokenPath, serializeData);
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