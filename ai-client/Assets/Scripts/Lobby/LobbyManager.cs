using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;
using Utility;
using Newtonsoft.Json;
using TMPro;
using UnityEngine.UI;

public class LobbyManager : MonoBehaviour
{
    [SerializeField] private GameObject _roomPrefab;
    [SerializeField] private Transform _contentTr;
    [SerializeField] private Button _createRoomButton;
    [SerializeField] private TMP_InputField _roomNameField;

    private Dictionary<Guid, LobbyRoomObjectController> _rooms = new();
    private IEnumerator _updateRoutine;
    private IEnumerator _createRoomRoutine;

    private AuthManager _authManager;

    private LobbyCanvasController _lobbyCanvasController;

    private WaitForSeconds _waitSecond;

    public bool IsRequestedJoin { get; private set; }

    public void NotifyRequestJoin()
    {
        IsRequestedJoin = true;
    }

    public void NotifyRequestEndJoin()
    {
        IsRequestedJoin = false;
    }

    private void Awake()
    {
        _authManager = AuthManager.Instance;
        _createRoomButton.onClick.AddListener(OnClickCreateRoomButton);

        _lobbyCanvasController = transform.parent.GetComponent<LobbyCanvasController>();
        _waitSecond = new WaitForSeconds(0.5f);
    }

    private void OnClickCreateRoomButton()
    {
        string roomName = _roomNameField.text;
        if (string.IsNullOrEmpty(roomName))
            return;

        if (string.IsNullOrEmpty(_authManager.AccessToken))
            return;

        CreateRoom(roomName);
    }

    private void OnEnable()
    {
        InitLobby();
    }

    private void OnDisable()
    {
        if (_updateRoutine is not null)
        {
            StopCoroutine(_updateRoutine);
            _updateRoutine = null;
        }

        if (_createRoomRoutine is not null)
        {
            StopCoroutine(_createRoomRoutine);
            _createRoomRoutine = null;
        }
    }

    private void InitLobby()
    {
        _createRoomButton.interactable = true;
        _roomNameField.text = "";
        UpdateLobby();
    }

    private void UpdateLobby()
    {
        _updateRoutine = UpdateLobbyRoutine();
        StartCoroutine(_updateRoutine);
    }

    private IEnumerator UpdateLobbyRoutine()
    {
        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_GET_ALL;
        if (string.IsNullOrEmpty(_authManager.AccessToken))
            yield break;

        while (true)
        {
            using var request = new UnityWebRequest(apiUri, "GET");
            request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
            request.SetRequestHeader("Content-Type", "application/json");
            request.downloadHandler = new DownloadHandlerBuffer();

            yield return request.SendWebRequest();

            if (request.result == UnityWebRequest.Result.Success)
            {
                List<GroupDto> response = request.responseCode == 204 ?
                    new List<GroupDto>() : JsonConvert.DeserializeObject<List<GroupDto>>(request.downloadHandler.text);
                UpdateRooms(response);
            }
            else if (request.result == UnityWebRequest.Result.ConnectionError)
            {
                Debug.Log($"서버와 연결에 실패하였습니다.");
            }
            else
            {
                Debug.LogError($"Fatal Error in Update Lobby Response : {request.responseCode}");
            }

            yield return _waitSecond;
        }
    }

    private void UpdateRooms(List<GroupDto> rooms)
    {
        HashSet<Guid> serverRoomIds = rooms.Select(r => r.groupId).ToHashSet();
        List<Guid> localRoomIds = _rooms.Keys.ToList();

        foreach (Guid roomId in localRoomIds.Where(roomId => !serverRoomIds.Contains(roomId)))
        {
            if (!_rooms.TryGetValue(roomId, out LobbyRoomObjectController roomController)) continue;
            Destroy(roomController.gameObject);
            _rooms.Remove(roomId);
        }

        foreach (GroupDto room in rooms)
        {
            if (_rooms.TryGetValue(room.groupId, out LobbyRoomObjectController roomController))
            {
                roomController.UpdateRoomState(room);
            }
            else
            {
                var newRoomController = Instantiate(_roomPrefab, _contentTr).GetComponent<LobbyRoomObjectController>();
                newRoomController.UpdateRoomState(room);
                _rooms.Add(room.groupId, newRoomController);
            }
        }
    }

    private void CreateRoom(string roomName)
    {
        if (_createRoomRoutine is not null)
            return;

        _createRoomRoutine = CreateRoomRoutine(roomName);
        StartCoroutine(_createRoomRoutine);
    }

    private IEnumerator CreateRoomRoutine(string roomName)
    {
        _createRoomButton.interactable = false;
        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_CREATE;
        var requester = new UserSimpleDto
        {
            username = _authManager.Username,
            uid = _authManager.UserGuid
        };

        var requestDto = new CreateGroupRequestDto
        {
            groupName = roomName,
            requester = requester
        };

        string requestJson = JsonConvert.SerializeObject(requestDto);
        byte[] bodyRaw = Encoding.UTF8.GetBytes(requestJson);

        using var request = new UnityWebRequest(apiUri, "POST");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.downloadHandler = new DownloadHandlerBuffer();
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);

        yield return request.SendWebRequest();

        if (request.result == UnityWebRequest.Result.Success)
        {
            var response = JsonConvert.DeserializeObject<GroupDto>(request.downloadHandler.text);
            _lobbyCanvasController.ChangePanelToRoomPanel(response);
        }
        else if (request.result == UnityWebRequest.Result.ConnectionError)
        {
            Debug.Log($"서버와 연결에 실패하였습니다.");
        }
        else
        {
            Debug.LogError($"Fatal Error in Create Room Response: {request.responseCode}");
        }
        _createRoomRoutine = null;
        _createRoomButton.interactable = true;
    }
}