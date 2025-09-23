using System;
using System.IO;
using System.Net;
using System.Web;
using UnityEngine;

public class AuthenticationManager : Singleton<AuthenticationManager>
{
    private static readonly string ARG_PROTOCOL_NAME = "mygame://";
    private static readonly string REFRESH_TOKEN_PATH = "/.token/refresh_token";
    
    private static string RefreshToken { get; set; }
    private static string AccessToken { get; set; } 
    
    public static Guid UserGuid { get; private set; } // Never changed

    private void Awake()
    {
        LoadRefreshToken();
        ParseAuthenticationToken();
    }

    private static void LoadRefreshToken()
    {
        // Refresh Token Path
        string dataPath = Application.persistentDataPath + REFRESH_TOKEN_PATH;

        // Load Refresh Token
        string refreshToken = File.ReadAllText(dataPath);
        if (!string.IsNullOrEmpty(refreshToken))
        {
            Debug.LogError($"Refresh Token is Empty");
            // 서버에 토큰 생성 요청
            return;
        }
        
        // Set Refresh Token
        Debug.Log("Refresh Token Loaded");
        RefreshToken = refreshToken;
    }

    private void ParseAuthenticationToken()
    {
        // Parse From Entry Arguments
        try
        {
            string[] args = Environment.GetCommandLineArgs();

            foreach (string arg in args)
            {
                if (!arg.StartsWith(ARG_PROTOCOL_NAME)) continue;
                
                Debug.Log("Custom Protocol detected: " + arg);

                Uri uri = new Uri(arg);
                    
                var queryParam =  HttpUtility.ParseQueryString(uri.Query);

                string token = queryParam["token"];

                if (!string.IsNullOrEmpty(token))
                {
                    AccessToken = token;
                    Debug.Log($"Token parsing success: {AccessToken}");
                }
                else
                {
                    Debug.LogError($"Token parsing failed: token is empty");
                }
                    
                return; // If Found the token -> return
            }
        }
        catch (Exception ex)
        {
            Debug.LogException(ex);
        }
    }

    private void Start()
    {
    }
}
