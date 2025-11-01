using System.Collections;
using UnityEngine;
using System.Collections.Generic;
using System.Text;
using Newtonsoft.Json;
using TMPro;
using UnityEngine.Networking;
using UnityEngine.UI;
using Utility;
using Network;
using NetworkData;

public class InRoomManager : MonoBehaviour
{
    private readonly List<GameObject> _players = new(4);

    [SerializeField] private Button _startButton;
    [SerializeField] private Button _exitButton;
    [SerializeField] private TMP_Text _roomName;

    private InternalGroupDto _internalGroupInfo = null;
    private GroupDto _externalGroupInfo = null;

    private IEnumerator _roomUpdateRoutine = null;
    private IEnumerator _waitSignalRoutine = null;
    private IEnumerator _startGameRoutine = null;
    private IEnumerator _roomExitRoutine = null;

    private AuthManager _authManager;
    private LogicServerConnector _logicServerConnector;

    private WaitForSeconds _waitRoomUpdate;
    private WaitForSeconds _waitRoomStatus;

    private LobbyCanvasController _lobbyCanvasController;

    private void Awake()
    {
        _authManager = AuthManager.Instance;
        _logicServerConnector = LogicServerConnector.Instance;

        for (var i = 0; i < 4; ++i)
        {
            _players.Add(transform.GetChild(0).GetChild(i).gameObject);
        }

        _waitRoomUpdate = new WaitForSeconds(0.3f);
        _waitRoomStatus = new WaitForSeconds(5.0f);
        _exitButton.onClick.AddListener(OnClickExitButton);
        _startButton.onClick.AddListener(OnClickStartButton);
        _lobbyCanvasController = transform.parent.GetComponent<LobbyCanvasController>();
    }

    public void InitRoom(InternalGroupDto groupDto)
    {
        _internalGroupInfo = groupDto;

        // Internal Group Dto to Protobuf Group Dto
        var owner = new NetworkData.UserSimpleDto()
        {
            Uid = groupDto.Owner.Uid.ToString(),
            Username = groupDto.Owner.Username
        };

        _externalGroupInfo = new GroupDto()
        {
            GroupId = groupDto.GroupId.ToString(),
            Name = groupDto.Name,
            Owner = owner,
        };

        foreach (var item in groupDto.Players)
        {
            var player = new NetworkData.UserSimpleDto()
            {
                Uid = item.Uid.ToString(),
                Username = item.Username,
            };

            _externalGroupInfo.PlayerList.Add(player);
        }

        _roomName.text = groupDto.Name;

        _startButton.interactable = _authManager.UserGuid == _internalGroupInfo.Owner.Uid;
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

        _externalGroupInfo = null;
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
        if (string.IsNullOrEmpty(_authManager.AccessToken) || _internalGroupInfo is null || _internalGroupInfo.Owner.Uid != _authManager.UserGuid)
        {
            Debug.Log($"Access Token or InternalGroup or Owner Invalid");
            yield break;
        }

        Debug.Log($"Passed");
        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_MATCHMAKING_START;

        var requestString = JsonConvert.SerializeObject(_internalGroupInfo.Owner);
        byte[] bodyRaw = Encoding.UTF8.GetBytes(requestString);

        // Header, Simple User Dto Body
        using var request = new UnityWebRequest(apiUri, "POST");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("groupId", _internalGroupInfo.GroupId.ToString());
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
        if (string.IsNullOrEmpty(_authManager.AccessToken) || _internalGroupInfo is null)
            yield break;

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_LEAVE;
        var requestString = $"{{\"groupId\":\"{_internalGroupInfo.GroupId}\",\"userId\":\"{_authManager.UserGuid}\"}}";
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
        if (string.IsNullOrEmpty(_authManager.AccessToken) || _internalGroupInfo is null)
            yield break;

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_GET_INFO;
        while (true)
        {
            var requestString = $"\"{_internalGroupInfo.GroupId}\"";
            byte[] bodyRaw = Encoding.UTF8.GetBytes(requestString);

            using var request = new UnityWebRequest(apiUri, "GET");
            request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
            request.SetRequestHeader("Content-Type", "application/json");
            request.downloadHandler = new DownloadHandlerBuffer();
            request.uploadHandler = new UploadHandlerRaw(bodyRaw);

            yield return request.SendWebRequest();

            if (request.result == UnityWebRequest.Result.Success)
            {
                var response = JsonConvert.DeserializeObject<InternalGroupDto>(request.downloadHandler.text);
                _internalGroupInfo = response;

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

            yield return _waitRoomUpdate;
        }
    }

    private void UpdateRoomInfo()
    {
        if (_roomUpdateRoutine is not null || _internalGroupInfo is null)
            return;

        _roomUpdateRoutine = RoomUpdateRoutine();
        StartCoroutine(_roomUpdateRoutine);
    }

    private void UpdatePlayersInfo()
    {
        var users = _internalGroupInfo.Players;
        Debug.Assert(users.Count <= 4, "Inv");
        for (var i = 0; i < _players.Count; ++i)
        {
            UpdateUserSlot(i, i < users.Count ? users[i] : null);
        }
    }

    private void UpdateUserSlot(int idx, Utility.UserSimpleDto user)
    {
        if (user is null)
        {
            _players[idx].transform.GetChild(0).GetComponent<TMP_Text>().text = "";
            return;
        }

        string prefix = "";

        if (user.Uid == _internalGroupInfo.Owner.Uid)
            prefix = "(Owner) ";

        if (user.Uid == _authManager.UserGuid)
            prefix += "(You) ";

        _players[idx].transform.GetChild(0).GetComponent<TMP_Text>().text = prefix + user.Username;
    }

    private void WaitStartSignal()
    {
        if (_waitSignalRoutine is not null || _internalGroupInfo is null)
            return;

        _waitSignalRoutine = WaitSignalRoutine();
        StartCoroutine(_waitSignalRoutine);
    }

    private IEnumerator WaitSignalRoutine()
    {
        Debug.Assert(_internalGroupInfo is not null);
        Debug.Assert(!string.IsNullOrEmpty(_authManager.AccessToken));

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_MATCHMAKING_CHECKSTATUS;

        using var request = new UnityWebRequest(apiUri, "GET");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.SetRequestHeader("groupId", _internalGroupInfo.GroupId.ToString());
        request.downloadHandler = new DownloadHandlerBuffer();
        request.timeout = 30;

        string ip = null;
        ushort port = 0;
        while (_internalGroupInfo is not null)
        {
            yield return request.SendWebRequest(); // Long-Polling
            var response = JsonConvert.DeserializeObject<GroupStatusResponseDto>(request.downloadHandler.text);

            // 성공 응답 이외의 모든 응답은 오류로 처리
            if (request.result != UnityWebRequest.Result.Success)
            {
                Debug.LogError($"오류 발생 {request.responseCode}");
                yield return _waitRoomStatus;
                continue;
            }

            // timeout 여부 파악
            if (!response.status)
            {
                Debug.Log("timeout. wait again");
                continue;
            }

            Debug.Log($"Success logic Server info {response.serverIp}:{response.port}");
            ip = response.serverIp;
            port = response.port;
            break;
        }

        Debug.Log($"Connect to Logic Server");

        // 로직 서버 접속 시도 (접속 실패 시 예외 처리 필요)
        var connectTask = _logicServerConnector.TryConnectToServer(this, _externalGroupInfo, ip, port);
        yield return new WaitUntil(() => connectTask.IsCompleted);

        var result = connectTask.Result;
        if (!result)
        {
            _waitSignalRoutine = null;
            InitByFailedConnection();
            yield break;
        }

        if (_startGameRoutine is not null)
        {
            StopCoroutine(_startGameRoutine);
            _startGameRoutine = null;
        }

        if (_roomUpdateRoutine is not null)
        {
            StopCoroutine(_roomUpdateRoutine);
            _roomUpdateRoutine = null;
        }

        _waitSignalRoutine = null;
        SceneController.Instance.LoadSceneAsync(SceneController.EScene.GameScene);
        Debug.Log($"Scene Load call pass");
    }

    public void InitByFailedConnection()
    {
        UpdateRoomInfo();
        WaitStartSignal();
    }
}