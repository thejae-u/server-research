using System;
using System.Collections;
using UnityEngine;
using System.Collections.Generic;
using System.Text;
using Newtonsoft.Json;
using TMPro;
using UnityEngine.Networking;
using UnityEngine.UI;
using Utility;

public class InRoomManager : MonoBehaviour
{
    private readonly List<GameObject> _players = new(4);

    [SerializeField] private Button _startButton;
    [SerializeField] private Button _exitButton;
    [SerializeField] private TMP_Text _roomName;

    private GroupDto _currentGroup = null;

    private IEnumerator _roomUpdateRoutine;
    private IEnumerator _waitSignalRoutine;
    private IEnumerator _startGameRoutine;
    private IEnumerator _roomExitRoutine;

    private AuthManager _authManager;
    private WaitForSeconds _waitSecond;

    private LobbyCanvasController _lobbyCanvasController;

    private void Awake()
    {
        _authManager = AuthManager.Instance;

        for (var i = 0; i < 4; ++i)
        {
            _players.Add(transform.GetChild(0).GetChild(i).gameObject);
        }

        _waitSecond = new WaitForSeconds(0.3f);
        _exitButton.onClick.AddListener(OnClickExitButton);
        _startButton.onClick.AddListener(OnClickStartButton);
        _lobbyCanvasController = transform.parent.GetComponent<LobbyCanvasController>();
    }

    public void InitRoom(GroupDto groupDto)
    {
        _currentGroup = groupDto;
        _roomName.text = groupDto.name;

        _startButton.interactable = _authManager.UserGuid == _currentGroup.owner.uid;
        UpdateRoomInfo();
        WaitStartSignal();
    }

    private void OnEnable()
    {
        UpdateRoomInfo();
        WaitStartSignal();
    }

    private void OnDisable()
    {
        if (_roomUpdateRoutine is not null)
        {
            StopCoroutine(_roomUpdateRoutine);
            _roomUpdateRoutine = null;
        }

        if (_waitSignalRoutine is not null)
        {
            StopCoroutine(_waitSignalRoutine);
            _waitSignalRoutine = null;
        }

        _currentGroup = null;
    }

    private void OnClickStartButton()
    {
        if (_startGameRoutine is not null)
            return;

        _startGameRoutine = StartGameRoutine();
        StartCoroutine(_startGameRoutine);
    }

    private void OnClickExitButton()
    {
        ExitRoom();
    }

    private void ExitRoom()
    {
        if (_roomExitRoutine is not null)
            return;

        _roomExitRoutine = ExitRoutine();
        StartCoroutine(_roomExitRoutine);
    }

    private IEnumerator StartGameRoutine()
    {
        if (string.IsNullOrEmpty(_authManager.AccessToken) || _currentGroup is null || _currentGroup.owner.uid != _authManager.UserGuid)
            yield break;

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_MATCHMAKING_START;

        var requestString = JsonConvert.SerializeObject(_currentGroup.owner);
        byte[] bodyRaw = Encoding.UTF8.GetBytes(requestString);

        // Header, Simple User Dto Body
        var request = WebServerUtils.GetAuthorizeRequestBase(apiUri, EHttpMethod.POST, _authManager.AccessToken);
        request.SetRequestHeader("groupId", _currentGroup.groupId.ToString());
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);
        request.downloadHandler = new DownloadHandlerBuffer();

        yield return request.SendWebRequest();

        if (request.result == UnityWebRequest.Result.ConnectionError)
        {
            Debug.LogError("서버와 연결할 수 없음");
            yield break;
        }
    }

    private IEnumerator ExitRoutine()
    {
        if (string.IsNullOrEmpty(_authManager.AccessToken) || _currentGroup is null)
            yield break;

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_LEAVE;
        var requestString = $"{{\"groupId\":\"{_currentGroup.groupId}\",\"userId\":\"{_authManager.UserGuid}\"}}";
        byte[] bodyRaw = Encoding.UTF8.GetBytes(requestString);

        var request = WebServerUtils.GetAuthorizeRequestBase(apiUri, EHttpMethod.POST, _authManager.AccessToken);
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);
        request.downloadHandler = new DownloadHandlerBuffer();

        yield return request.SendWebRequest();

        switch (request.result)
        {
            case UnityWebRequest.Result.Success:
                _lobbyCanvasController.ChangePanelToLobbyPanel();
                _roomExitRoutine = null;
                break;

            case UnityWebRequest.Result.ConnectionError:
                Debug.LogError($"서버와 통신에 실패하였습니다.");
                break;

            case UnityWebRequest.Result.ProtocolError:
            case UnityWebRequest.Result.DataProcessingError:
            case UnityWebRequest.Result.InProgress:
            default:
                Debug.LogError($"Fatal Error in Exit Room Response: {request.responseCode}");
                break;
        }

        _roomExitRoutine = null;
    }

    private IEnumerator RoomUpdateRoutine()
    {
        if (string.IsNullOrEmpty(_authManager.AccessToken) || _currentGroup is null)
            yield break;

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_GET_INFO;
        while (true)
        {
            var requestString = $"\"{_currentGroup.groupId}\"";
            byte[] bodyRaw = Encoding.UTF8.GetBytes(requestString);

            var request = WebServerUtils.GetAuthorizeRequestBase(apiUri, EHttpMethod.GET, _authManager.AccessToken);
            request.uploadHandler = new UploadHandlerRaw(bodyRaw);
            request.downloadHandler = new DownloadHandlerBuffer();

            yield return request.SendWebRequest();

            switch (request.result)
            {
                case UnityWebRequest.Result.Success:
                    var response = JsonConvert.DeserializeObject<GroupDto>(request.downloadHandler.text);
                    _currentGroup = response;

                    UpdatePlayersInfo();
                    break;

                case UnityWebRequest.Result.ConnectionError:
                    Debug.Log("서버와 연결에 실패하였습니다.");
                    break;

                case UnityWebRequest.Result.ProtocolError:
                case UnityWebRequest.Result.DataProcessingError:
                case UnityWebRequest.Result.InProgress:
                default:
                    Debug.LogError($"Fatal Error in Room Update Response: {request.responseCode}");
                    Debug.Log($"Current room Info: {_currentGroup.name}, {_currentGroup.groupId}");
                    break;
            }

            yield return _waitSecond;
        }
    }

    private void UpdateRoomInfo()
    {
        if (_roomUpdateRoutine is not null || _currentGroup is null)
            return;

        _roomUpdateRoutine = RoomUpdateRoutine();
        StartCoroutine(_roomUpdateRoutine);
    }

    private void UpdatePlayersInfo()
    {
        var users = _currentGroup.players;
        Debug.Assert(users.Count <= 4, "Inv");
        for (var i = 0; i < _players.Count; ++i)
        {
            UpdateUserSlot(i, i < users.Count ? users[i] : null);
        }
    }

    private void UpdateUserSlot(int idx, UserSimpleDto user)
    {
        if (user is null)
        {
            _players[idx].transform.GetChild(0).GetComponent<TMP_Text>().text = "";
            return;
        }

        string prefix = "";

        if (user.uid == _currentGroup.owner.uid)
            prefix = "(Owner) ";

        if (user.uid == _authManager.UserGuid)
            prefix += "(You) ";

        _players[idx].transform.GetChild(0).GetComponent<TMP_Text>().text = prefix + user.username;
    }

    private void WaitStartSignal()
    {
        if (_waitSignalRoutine is not null || _currentGroup is null)
            return;

        _waitSignalRoutine = WaitSignalRoutine();
        StartCoroutine(_waitSignalRoutine);
    }

    private IEnumerator WaitSignalRoutine()
    {
        Debug.Assert(_currentGroup is not null);

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_MATCHMAKING_CHECKSTATUS;
        var request = WebServerUtils.GetAuthorizeRequestBase(apiUri, EHttpMethod.GET, _authManager.AccessToken);
        request.SetRequestHeader("groupId", _currentGroup.groupId.ToString());

        while (_currentGroup is not null)
        {
            yield return request.SendWebRequest(); // Long-Polling

            switch (request.result)
            {
                case UnityWebRequest.Result.Success:
                    var response = JsonConvert.DeserializeObject<GroupStatusResponseDto>(request.downloadHandler.text);
                    if (response.status)
                    {
                        // 서버 정보 가공해서 Scene 넘기기
                    }
                    break;

                case UnityWebRequest.Result.ConnectionError:
                    Debug.Log("서버에 연결 할 수 없음");
                    yield break;

                case UnityWebRequest.Result.ProtocolError:
                case UnityWebRequest.Result.DataProcessingError:
                case UnityWebRequest.Result.InProgress:
                default:
                    Debug.LogError($"알 수 없는 오류 {request.responseCode} : {request.downloadHandler.text}");
                    yield break;
            }
        }
    }
}