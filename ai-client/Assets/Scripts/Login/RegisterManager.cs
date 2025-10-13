using System.Collections;
using System.Text;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.UI;
using Utility;

public class RegisterManager : MonoBehaviour
{
    [SerializeField] private TMP_InputField _usernameField;
    [SerializeField] private TMP_InputField _passwordField;
    [SerializeField] private TMP_InputField _confirmPasswordField;

    [SerializeField] private Button _registerButton;

    [SerializeField] private TMP_Text _statusText;

    [SerializeField] private Button _backButton;

    private IEnumerator _toastRoutine;
    private IEnumerator _registerRoutine;

    private void Awake()
    {
        _registerButton.onClick.AddListener(OnClickRegisterButton);
        _backButton.onClick.AddListener(OnBackButtonClick);

        _toastRoutine = null;
        _registerRoutine = null;
    }

    private void OnBackButtonClick()
    {
        transform.parent.gameObject.SetActive(false);
    }

    private void OnClickRegisterButton()
    {
        string username = _usernameField.text;
        string password = _passwordField.text;
        string confirmPassword = _confirmPasswordField.text;

        if (string.IsNullOrEmpty(username) || string.IsNullOrEmpty(password) || string.IsNullOrEmpty(confirmPassword))
        {
            // Toast Text
            SetStatusText($"모든 필드는 필수입니다.", Color.red, 3.0f);
            return;
        }

        if (password.Length < 8 || confirmPassword.Length < 8)
        {
            SetStatusText($"비밀번호는 8자 이상이어야 합니다.", Color.red, 3.0f);
            return;
        }

        if (!string.Equals(password, confirmPassword))
        {
            SetStatusText($"비밀번호가 일치하지 않습니다.", Color.red, 3.0f);
            return;
        }

        if (_registerRoutine is not null)
            return;

        _registerRoutine = RegisterRoutine(username, password);
        StartCoroutine(_registerRoutine);
    }

    private IEnumerator RegisterRoutine(string username, string password)
    {
        _registerButton.interactable = false;

        var jsonBodyString = $"{{\"username\": \"{username}\", \"password\": \"{password}\"}}";
        byte[] bodyRaw = Encoding.UTF8.GetBytes(jsonBodyString);
        string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_AUTH_REGISTER;

        var isSuccess = false;
        var request = WebServerUtils.GetUnauthorizeRequestBase(apiUri, EHttpMethod.POST);
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);

        _statusText.text = "회원가입 중...";
        _statusText.color = Color.black;

        yield return request.SendWebRequest();

        switch (request.result)
        {
            case UnityWebRequest.Result.Success:
                // 로그인으로 유도 -> Login canvas로 변경
                isSuccess = true;
                break;

            case UnityWebRequest.Result.ConnectionError:
                SetStatusText($"서버와 연결에 실패했습니다.", Color.red, 5.0f);
                break;

            case UnityWebRequest.Result.InProgress:
                break;

            case UnityWebRequest.Result.ProtocolError:
            case UnityWebRequest.Result.DataProcessingError:
            default:
                SetStatusText($"이미 존재하는 아이디입니다.", Color.blue, 3.0f);
                break;
        }

        _registerButton.interactable = true;
        _registerRoutine = null;

        if (!isSuccess) yield break;

        // Switch Login Canvas
        ResetCanvas();
        transform.parent.gameObject.SetActive(false);
    }

    private void SetStatusText(string text, Color color, float duration)
    {
        _statusText.text = text;
        _statusText.color = color;

        if (_toastRoutine is not null)
        {
            StopCoroutine(_toastRoutine);
        }

        _toastRoutine = TextToastRoutine(duration);
        StartCoroutine(_toastRoutine);
    }

    private IEnumerator TextToastRoutine(float duration)
    {
        float curTime = 0;
        while (curTime <= duration)
        {
            yield return null;
            curTime += Time.deltaTime;
        }

        _statusText.text = "";
        _statusText.color = Color.black;

        _toastRoutine = null;
    }

    private void ResetCanvas()
    {
        _usernameField.text = "";
        _passwordField.text = "";
        _confirmPasswordField.text = "";
        _statusText.text = "";
    }
}