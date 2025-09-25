using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Networking;
using Utility;
using Newtonsoft.Json;

public class LobbyManager : MonoBehaviour
{
    [SerializeField] private GameObject _roomPrefab;
    [SerializeField] private Transform _contentTr;

    private Dictionary<Guid, RoomController> _rooms = new();
    private IEnumerator _updateRoutine;

    private WaitForSeconds _waitForSeconds;

    private AuthManager _authManager;
    
    private void Awake()
    {
        _authManager = AuthManager.Instance;
        
        _waitForSeconds = new WaitForSeconds(1.0f);
        
        InitLobby();
    }

    private void InitLobby()
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
            using UnityWebRequest request = UnityWebRequest.Get(apiUri);
            request.SetRequestHeader("Authorization", $"Bearer {_authManager.AccessToken}");
            yield return request.SendWebRequest();
            
            switch (request.result)
            {
                case UnityWebRequest.Result.Success:
                    List<GroupDto> response = request.responseCode == 204 ? 
                        new List<GroupDto>() : JsonConvert.DeserializeObject<List<GroupDto>>(request.downloadHandler.text);
                    UpdateRooms(response);
                    break;
                case UnityWebRequest.Result.ConnectionError:
                    Debug.Log($"서버와 연결에 실패하였습니다.");
                    break;
                
                case UnityWebRequest.Result.ProtocolError:
                case UnityWebRequest.Result.DataProcessingError:
                case UnityWebRequest.Result.InProgress:
                default:
                    Debug.LogError($"Fatal Error in Update Lobby Response");
                    break;
            }
            
            yield return _waitForSeconds;
        }
    }
    
    private void UpdateRooms(List<GroupDto> rooms)
    {
        var serverRoomIds = rooms.Select(r => r.groupId).ToHashSet();
        var localRoomIds = _rooms.Keys.ToList();

        foreach (var roomId in localRoomIds)
        {
            if (!serverRoomIds.Contains(roomId))
            {
                if(_rooms.TryGetValue(roomId, out RoomController roomController))
                {
                    Destroy(roomController.gameObject);
                    _rooms.Remove(roomId);
                }
            }
        }

        foreach (var room in rooms)
        {
            if (_rooms.TryGetValue(room.groupId, out RoomController roomController))
            {
                roomController.UpdateRoomState(room);
            }
            else
            {
                var newRoomController = Instantiate(_roomPrefab, _contentTr).GetComponent<RoomController>();
                newRoomController.UpdateRoomState(room);
                _rooms.Add(room.groupId, newRoomController);
            }
        }
    }
}
