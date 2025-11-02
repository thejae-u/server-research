using System.Collections;
using System.Text;
using UnityEngine;
using UnityEngine.UI;
using TMPro;
using UnityEngine.Networking;
using Utility;
using Newtonsoft.Json;

public class LoginManager : MonoBehaviour
{
    [SerializeField] private TMP_InputField _usernameField;
    [SerializeField] private TMP_InputField _passwordField;
    [SerializeField] private Button _loginButton;
    [SerializeField] private Button _registerButton;

    [SerializeField] private GameObject _registerCanvas;
    [SerializeField] private GameObject _accessCanvas;

    [SerializeField] private TMP_Text _statusText;

    private IEnumerator _toastRoutine;
    private IEnumerator _loginRoutine;

    private AuthManager _authManager;

    private void Awake()
    {
        _loginButton.onClick.AddListener(OnClickLoginButton);
        _registerButton.onClick.AddListener(OnClickRegisterButton);

        _statusText.text = "";

        _authManager = AuthManager.Instance;
    }

    private void Start()
    {
        // 이미 인증 토큰이 있는 경우 접속 버튼이 있는 창을 띄움
        StartCoroutine(WaitToken());
    }

    private IEnumerator WaitToken()
    {
        yield return null;

        if (_authManager.HasRefreshToken)
        {
            _accessCanvas.SetActive(true);
        }
    }

    private void OnClickLoginButton()
    {
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
        SetStatusText($"로그인 중...", Color.black);
        _loginButton.interactable = false;

        var jsonBodyString = $"{{\"username\": \"{username}\", \"password\": \"{password}\"}}";
        byte[] bodyRaw = Encoding.UTF8.GetBytes(jsonBodyString);
        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_AUTH_LOGIN;

        using var request = new UnityWebRequest(apiUri, "POST");
        request.SetRequestHeader("Content-Type", "application/json");
        request.downloadHandler = new DownloadHandlerBuffer();
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);

        yield return request.SendWebRequest();

        if (request.result == UnityWebRequest.Result.Success) // 200 (OK)
        {
            string jsonResponse = request.downloadHandler.text;
            var response = JsonConvert.DeserializeObject<LoginResponse>(jsonResponse);

            _authManager.InitTokens(response);
            ToastStatusText("로그인 성공, 게임에 접속합니다.", new Color(0, 128, 0), 1.0f);

            StartCoroutine(ChangeToLobbyScene());
        }
        else if (request.result == UnityWebRequest.Result.ConnectionError)
        {
            ToastStatusText("서버와 연결에 실패했습니다.", Color.red, 5.0f);
        }
        else
        {
            ToastStatusText("아이디 또는 비밀번호가 틀렸습니다.", Color.red, 5.0f);
        }

        _loginButton.interactable = true;
        _loginRoutine = null;
    }

    private void OnClickRegisterButton()
    {
        _registerCanvas.SetActive(true);
    }

    private IEnumerator ChangeToLobbyScene()
    {
        _loginButton.interactable = false;

        while (_toastRoutine is not null)
        {
            yield return null;
        }

        SetStatusText("접속중...", Color.black);
        SceneController.Instance.LoadSceneAsync(SceneController.EScene.LobbyScene);
    }

    private void SetStatusText(string text, Color color)
    {
        _statusText.text = text;
        _statusText.color = color;
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