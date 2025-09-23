using System.Collections;
using System.Text;
using UnityEngine;
using UnityEngine.UI;
using TMPro;
using UnityEngine.Networking;
using Utility;

public class LoginManager : MonoBehaviour
{
    [SerializeField] private TMP_InputField _usernameField;
    [SerializeField] private TMP_InputField _passwordField;
    [SerializeField] private Button _loginButton;
    [SerializeField] private Button _registerButton;

    [SerializeField] private GameObject _registerCanvas;

    [SerializeField] private TMP_Text _statusText;

    private IEnumerator _toastRoutine;
    private IEnumerator _loginRoutine;

    [System.Serializable]
    private class LoginResponse
    {
        public string accessToken;
        public string refreshToken;
    }

    private void Awake()
    {
        _loginButton.onClick.AddListener(OnClickLoginButton); 
        _registerButton.onClick.AddListener(OnClickRegisterButton);

        _statusText.text = "";
    }

    private void OnClickLoginButton()
    {
        // Http Method to Web Server

        string username = _usernameField.text;
        string password = _passwordField.text;

        if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password))
        {
            ToastStatusText($"모든 필드는 필수입니다.", Color.red, 3.0f);
            return;
        }
        
        if (_loginRoutine is not null)
            return;

        _loginRoutine = LoginRoutine(username, password);
        StartCoroutine(_loginRoutine);
    }

    private IEnumerator LoginRoutine(string username, string password)
    {
        _loginButton.interactable = false;
        
        var jsonBodyString = $"{{\"username\": \"{username}\", \"password\": \"{password}\"}}";
        byte[] bodyRaw = Encoding.UTF8.GetBytes(jsonBodyString);
        string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_AUTH_LOGIN;

        using (var request = new UnityWebRequest(apiUri, "POST"))
        {
            request.uploadHandler = new UploadHandlerRaw(bodyRaw);
            request.downloadHandler = new DownloadHandlerBuffer();
            request.SetRequestHeader("Content-Type", "application/json");
            
            yield return request.SendWebRequest();

            if (request.result == UnityWebRequest.Result.Success) // 200 (OK)
            {
                string jsonResponse = request.downloadHandler.text;
                var response = JsonUtility.FromJson<LoginResponse>(jsonResponse);

                Debug.Log($"access token is: \n{response.accessToken}");
                Debug.Log($"refresh token is:\n{response.refreshToken}");
            }
            else if (request.result == UnityWebRequest.Result.ConnectionError)
            {
                ToastStatusText("서버와 연결에 실패했습니다.", Color.red, 5.0f);
            }
            else
            {
                ToastStatusText("아이디 또는 비밀번호가 틀렸습니다.", Color.black, 5.0f);
            }
        }
        
        _loginButton.interactable = true;
        _loginRoutine = null;
    }

    private void OnClickRegisterButton()
    {
        _registerCanvas.SetActive(true);
    }

    private void ToastStatusText(string text, Color color, float duration = 0)
    {
        _statusText.text = text;
        _statusText.color = color;

        // No duration
        if (Mathf.Approximately(duration, 0.0f))
        {
            return;
        }

        if (_toastRoutine is not null)
        {
            StopCoroutine(_toastRoutine);
            _toastRoutine = null;
        }
        
        _toastRoutine = ToastTextRoutine(duration);
        StartCoroutine(_toastRoutine);
    }

    private IEnumerator ToastTextRoutine(float duration)
    {
        float curTime = 0;

        while (curTime <= duration)
        {
            yield return null;
            curTime += Time.deltaTime;
        }

        ResetStatusText();
        _toastRoutine = null;
    }

    private void ResetStatusText()
    {
        _statusText.text = "";
        _statusText.color = Color.black;
    }
}
