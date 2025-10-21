using System;
using System.Collections;
using System.Text;
using Newtonsoft.Json;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.UI;
using Utility;

public class LobbyRoomObjectController : MonoBehaviour
{
    private InternalGroupDto _groupDto;
    private Guid RoomGuid { get; set; }
    private TMP_Text _roomNameText;
    private TMP_Text _roomOwnerNameText;
    private TMP_Text _roomPlayerCountText;

    private Button _joinButton;

    private AuthManager _authManager;
    private IEnumerator _joinRoutine;

    private LobbyCanvasController RootCanvasController =>
        transform.parent.parent.parent.parent.parent.GetComponent<LobbyCanvasController>();

    private LobbyManager RootLobbyManager =>
        transform.parent.parent.parent.parent.GetComponent<LobbyManager>();

    public void UpdateRoomState(InternalGroupDto data)
    {
        if (data.Players is null)
        {
            Debug.LogError("users is null");
            return;
        }

        _groupDto = data;

        RoomGuid = data.GroupId;
        _roomNameText.text = data.Name;
        _roomOwnerNameText.text = data.Owner.Username;
        _roomPlayerCountText.text = $"{data.Players.Count}/4";
    }

    private void Awake()
    {
        _authManager = AuthManager.Instance;

        _joinButton = transform.GetChild(transform.childCount - 1).GetComponent<Button>();
        _joinButton.onClick.AddListener(OnClickJoinButton);

        _roomNameText = transform.GetChild(0).GetComponent<TMP_Text>();
        _roomOwnerNameText = transform.GetChild(1).GetComponent<TMP_Text>();
        _roomPlayerCountText = transform.GetChild(2).GetComponent<TMP_Text>();
    }

    private void OnClickJoinButton()
    {
        // Room Guid로 web Server에 접근 요청
        if (_joinRoutine is not null || RootLobbyManager.IsRequestedJoin)
            return;

        _joinRoutine = JoinRoomRoutine();
        StartCoroutine(_joinRoutine);
    }

    private IEnumerator JoinRoomRoutine()
    {
        if (string.IsNullOrEmpty(_authManager.AccessToken))
        {
            _joinRoutine = null;
            yield break;
        }

        RootLobbyManager.NotifyRequestJoin();

        const string apiUri = WebServerUtils.API_SERVER_IP + WebServerUtils.API_GROUP_JOIN;
        var requester = new UserSimpleDto
        {
            Uid = _authManager.UserGuid,
            Username = _authManager.Username
        };

        var requestDto = new JoinGroupRequestDto
        {
            groupId = RoomGuid,
            requester = requester
        };

        string requestJson = JsonConvert.SerializeObject(requestDto);
        byte[] bodyRaw = Encoding.UTF8.GetBytes(requestJson);

        using var request = new UnityWebRequest(apiUri, "POST");
        request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
        request.SetRequestHeader("Content-Type", "application/json");
        request.uploadHandler = new UploadHandlerRaw(bodyRaw);
        request.downloadHandler = new DownloadHandlerBuffer();

        yield return request.SendWebRequest();

        switch (request.result)
        {
            case UnityWebRequest.Result.Success:
                // Parsing GroupDto
                var response = JsonConvert.DeserializeObject<InternalGroupDto>(request.downloadHandler.text);
                _joinRoutine = null;

                RootCanvasController.ChangePanelToRoomPanel(response);
                RootLobbyManager.NotifyRequestEndJoin();
                break;

            case UnityWebRequest.Result.ConnectionError:
                Debug.LogError($"서버와 연결에 실패하였습니다.");
                break;

            case UnityWebRequest.Result.ProtocolError:
            case UnityWebRequest.Result.DataProcessingError:
            case UnityWebRequest.Result.InProgress:
            default:
                Debug.LogError($"Fatal Error in Join Room Response: {request.responseCode}");
                break;
        }

        _joinRoutine = null;
        RootLobbyManager.NotifyRequestEndJoin();
    }
}