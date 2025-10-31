using System;
using UnityEngine;
using System.Collections.Generic;
using Network;
using NetworkData;

public class SyncManager : Singleton<SyncManager>
{
    [SerializeField] private GameObject _syncObjectPrefab;
    [SerializeField] private GameObject _syncObjectNameTagPrefab;
    [SerializeField] private Transform _syncObjectCanvas;

    private LogicServerConnector _connector;
    private AuthManager _authManager;

    private readonly Dictionary<Guid, GameObject> _syncObjects = new();
    private readonly Dictionary<Guid, UserSimpleDto> _userInfos = new();

    private void Start()
    {
        _authManager = AuthManager.Instance;
        _connector = LogicServerConnector.Instance;

        // 시작 시 모든 플레이어 오브젝트 생성
        foreach (var user in _connector.Users)
        {
            var userId = Guid.Parse(user.Uid);
            var newUser = CreateSyncObject(user, Vector3.zero);
            if (!_syncObjects.TryAdd(userId, newUser))
            {
                Debug.LogError($"Invalid situation: {user.Uid} already exist in syncObjects");
            }

            if (!_userInfos.TryAdd(userId, user))
            {
                Debug.LogError($"Invalid situation: {user.Uid} already exist in userInfos");
            }
        }
    }

    private GameObject CreateSyncObject(UserSimpleDto user, Vector3 position)
    {
        GameObject syncObject = Instantiate(_syncObjectPrefab, position, Quaternion.identity); // Create the object

        syncObject.transform.SetParent(transform); // Set parent to SyncManager
        syncObject.name = user.ToString(); // Set the name to the objectId
        syncObject.GetComponent<SyncObject>().Init(user); // Initialize SyncObject

        // Create the name tag
        GameObject nameTag = Instantiate(_syncObjectNameTagPrefab, _syncObjectCanvas);
        nameTag.GetComponent<NameTagController>().Init(user, syncObject);

        _syncObjects.Add(Guid.Parse(user.Uid), syncObject); // Add to dict

        return syncObject;
    }

    public void SyncObjectPosition(Guid uid, MoveData moveData)
    {
        Debug.Log($"Received SyncObjectPosition - {uid} : {moveData.X}, {moveData.Y}, {moveData.Z}, Speed: {moveData.Speed}");
        if (uid == Guid.Empty)
        {
            Debug.Log($"Empty ObjectId received in SyncObjectPosition. Ignoring.");
            ++_connector.ErrorCount;
            return;
        }

        var startPosition = new Vector3(moveData.X, moveData.Y, moveData.Z);
        if (!_syncObjects.TryGetValue(uid, out var syncObject))
        {
            Debug.LogError($"Invalid Situation: {uid} is not exist in syncObjects");
            return;
        }

        var syncObjectComponent = syncObject.GetComponent<SyncObject>();
        syncObjectComponent.EnqueueMoveData(moveData);
    }

    public void SyncObjectNone(Guid objectId)
    {
        // self packet check and return
        if (objectId == _authManager.UserGuid)
            return;

        Debug.Log($"SyncObjectNone - {objectId}");
    }
}