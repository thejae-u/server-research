using System;
using System.Collections;
using System.Threading.Tasks;
using System.IO;
using Newtonsoft.Json;
using UnityEngine;
using Utility;
using System.Threading;

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

    public Task LoadingTask { get; private set; }
    private CancellationTokenSource _loadCancelToken;

    private void Awake()
    {
        _tokenPath = Application.persistentDataPath + REFRESH_TOKEN_PATH;
        //StartCoroutine(LoadRefreshToken());
        _loadCancelToken = new CancellationTokenSource();
        LoadingTask = LoadRefreshToken(_loadCancelToken.Token);
    }

    private void OnDestroy()
    {
        if (LoadingTask is not null && !LoadingTask.IsCompleted)
            _loadCancelToken.Cancel();

        _loadCancelToken.Dispose();
    }

    private async Task LoadRefreshToken(CancellationToken cToken)
    {
        try
        {
            if (!File.Exists(_tokenPath))
            {
                RefreshToken = string.Empty;
                return;
                //yield break;
            }

            //string loadedJson = File.ReadAllText(_tokenPath);
            string loadedJson = await File.ReadAllTextAsync(_tokenPath, cToken);

            // If Using Task -> await File.ReadAllTextAsync(_tokenPath);
            // Attach to Main Thread by await
            // Dispatch to Main Thread Again By UnitySynchronizationContext

            var deserializedData = JsonConvert.DeserializeObject<TokenInfo>(loadedJson);
            RefreshToken = deserializedData.refreshToken;
            Username = deserializedData.userName;
        }
        catch (OperationCanceledException)
        {
            Debug.Log($"Canceled loading token");
        }
        catch (Exception ex)
        {
            Debug.LogError($"Exception LoadRefreshToken: {ex.Message}");
        }
        finally
        {
            LoadingTask = null;
        }
    }

    public async void InitTokens(LoginResponse response)
    {
        RefreshToken = response.RefreshToken;
        AccessToken = response.AccessToken;
        UserGuid = Guid.Parse(response.User.Uid);
        Username = response.User.Username;

        await UpdateRefreshToken();
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

    private async Task UpdateRefreshToken()
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
            await File.WriteAllTextAsync(_tokenPath, serializeData);
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