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
        using var request = new UnityWebRequest(apiUri, "POST");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("groupId", _currentGroup.groupId.ToString());
        request.downloadHandler = new DownloadHandlerBuffer();
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);

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

        using var request = new UnityWebRequest(apiUri, "POST");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.downloadHandler = new DownloadHandlerBuffer();
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);

        yield return request.SendWebRequest();

        if (request.result == UnityWebRequest.Result.Success)
        {
            _lobbyCanvasController.ChangePanelToLobbyPanel();
            _roomExitRoutine = null;
        }
        else if (request.result == UnityWebRequest.Result.ConnectionError)
        {
            Debug.LogError($"서버와 통신에 실패하였습니다.");
        }
        else
        {
            Debug.LogError($"Fatal Error in Exit Room Response: {request.responseCode}");
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

            using var request = new UnityWebRequest(apiUri, "GET");
            request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
            request.SetRequestHeader("Content-Type", "application/json");
            request.downloadHandler = new DownloadHandlerBuffer();
            request.uploadHandler = new UploadHandlerRaw(bodyRaw);

            yield return request.SendWebRequest();

            if (request.result == UnityWebRequest.Result.Success)
            {
                var response = JsonConvert.DeserializeObject<GroupDto>(request.downloadHandler.text);
                _currentGroup = response;

                UpdatePlayersInfo();
            }
            else if (request.result == UnityWebRequest.Result.ConnectionError)
            {
                Debug.Log("서버와 연결에 실패하였습니다.");
            }
            else
            {
                Debug.LogError($"Fatal Error in Room Update Response: {request.responseCode}");
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
        Debug.Assert(!string.IsNullOrEmpty(_authManager.AccessToken));

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_MATCHMAKING_CHECKSTATUS;

        using var request = new UnityWebRequest(apiUri, "GET");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("groupId", _currentGroup.groupId.ToString());
        request.downloadHandler = new DownloadHandlerBuffer();
        request.timeout = 30;

        while (_currentGroup is not null)
        {
            yield return request.SendWebRequest(); // Long-Polling
            var response = JsonConvert.DeserializeObject<GroupStatusResponseDto>(request.downloadHandler.text);

            // 성공 응답 이외의 모든 응답은 오류로 처리
            if (request.result != UnityWebRequest.Result.Success)
            {
                Debug.LogError($"오류 발생 {request.responseCode}");
                yield return new WaitForSeconds(5);
                continue;
            }

            // timeout 여부 파악
            if (!response.status)
            {
                Debug.Log("timeout. wait again");
                continue;
            }

            Debug.Log($"Success logic Server info {response.serverIp}:{response.port}");
            yield break;
        }
    }
}