using System.Collections;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.SceneManagement;
using UnityEngine.UI;
using Utility;

public class AccessRefreshManager : MonoBehaviour
{
    [SerializeField] private Button _accessButton;
    [SerializeField] private Button _logoutButton;
    [SerializeField] private TMP_Text _userNameText;
    [SerializeField] private TMP_Text _statusText;

    private IEnumerator _accessRoutine;
    private IEnumerator _backToLoginRoutine;
    private IEnumerator _toastRoutine;

    private AuthManager _authManager;

    private void Awake()
    {
        _accessButton.onClick.AddListener(OnClickAccessButton);
        _logoutButton.onClick.AddListener(OnClickLogoutButton);

        _authManager = AuthManager.Instance;
        _statusText.text = "";
        _userNameText.text = $"로그인 된 아이디 : {_authManager.Username}";
    }

    private void OnClickAccessButton()
    {
        if (_accessRoutine is not null)
            return;

        _accessRoutine = AccessRoutine();
        StartCoroutine(_accessRoutine);
    }

    private void OnClickLogoutButton()
    {
        if (_backToLoginRoutine is not null)
        {
            StopCoroutine(_backToLoginRoutine);
            _backToLoginRoutine = null;
        }

        _backToLoginRoutine = BackToLoginRoutine(true);
        StartCoroutine(_backToLoginRoutine);
    }

    private IEnumerator AccessRoutine()
    {
        _statusText.text = "접속중...";
        _statusText.color = Color.black;

        _accessButton.interactable = false;

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_AUTH_REFRESH;

        var jsonBody = $"\"{_authManager.RefreshToken}\"";
        byte[] bodyRaw = Encoding.UTF8.GetBytes(jsonBody);

        using var request = new UnityWebRequest(apiUri, "POST");
        var request = WebServerUtils.GetUnauthorizeRequestBase(apiUri, EHttpMethod.POST);
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);

        yield return request.SendWebRequest();

        switch (request.result)
        {
            // 200 OK
            case UnityWebRequest.Result.Success:
                {
                    var response = JsonUtility.FromJson<LoginResponse>(request.downloadHandler.text);
                    _authManager.UpdateAccessToken(response);

                    StartCoroutine(ChangeToLobbyScene());
                    break;
                }
            case UnityWebRequest.Result.ConnectionError:
                ToastStatusText("서버와 연결에 실패했습니다.", Color.red, 5.0f);
                break;

            default:
                ToastStatusText("로그인 정보가 만료되었습니다.\n다시 로그인 해주세요.", Color.black, 3.0f);
                _backToLoginRoutine = BackToLoginRoutine();
                StartCoroutine(_backToLoginRoutine);
                break;
        }
    }

    private IEnumerator ChangeToLobbyScene()
    {
        ToastStatusText("로그인 성공, 게임에 접속합니다.", new Color(0, 128, 0), 1.0f);

        while (_toastRoutine is not null)
        {
            yield return null;
        }

        _statusText.text = "접속중...";
        _statusText.color = Color.black;

        AsyncOperation loginTask = SceneManager.LoadSceneAsync("LobbyScene");
        while (loginTask is { isDone: false })
        {
            yield return null;
        }
    }

    private IEnumerator BackToLoginRoutine(bool isLogout = false)
    {
        const float DURATION = 3.0f;
        if (!isLogout)
            ToastStatusText("로그인 정보가 만료되었습니다.\n다시 로그인 해주세요.", Color.black, DURATION);
        else
            ToastStatusText("로그아웃 되었습니다.\n로그인 페이지로 이동합니다.", Color.black, DURATION);

        while (_toastRoutine is not null)
        {
            yield return null;
        }

        _authManager.EraseAuthInformation();
        _backToLoginRoutine = null;

        gameObject.SetActive(false);
    }

    private void ToastStatusText(string text, Color color, float duration)
    {
        if (_toastRoutine is not null)
            StopCoroutine(_toastRoutine);

        _statusText.text = text;
        _statusText.color = color;

        _toastRoutine = ToastRoutine(duration);
        StartCoroutine(_toastRoutine);
    }

    private IEnumerator ToastRoutine(float duration)
    {
        if (Mathf.Approximately(duration, 0))
            yield break;

        float curTime = 0;
        while (curTime < duration)
        {
            yield return null;
            curTime += Time.deltaTime;
        }

        _statusText.text = "";
        _statusText.color = Color.black;
        _toastRoutine = null;
    }
}