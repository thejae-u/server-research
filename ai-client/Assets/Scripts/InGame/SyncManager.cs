using System;
using UnityEngine;
using System.Collections.Generic;
using Network;
using NetworkData;
using Random = UnityEngine.Random;

public class SyncManager : Singleton<SyncManager>
{
    [SerializeField] private GameObject _syncObjectPrefab;
    [SerializeField] private GameObject _syncObjectNameTagPrefab;
    [SerializeField] private Transform _syncObjectCanvas;

    public bool isManualMode = false;

    private LogicServerConnector _connector;
    private ManualConnector _manualConnector;
    private AuthManager _authManager;

    private readonly Dictionary<Guid, GameObject> _syncObjects = new();
    private readonly Dictionary<Guid, UserSimpleDto> _userInfos = new();

    private void Awake()
    {
    }

    private void Start()
    {
        if(isManualMode)
        {
            _manualConnector = ManualConnector.Instance;
            return;
        }

        _authManager = AuthManager.Instance;
        _connector = LogicServerConnector.Instance;

        if (_authManager is null || _connector is null)
        {
            return;
        }

        // 시작 시 모든 플레이어 오브젝트 생성
        foreach (var user in _connector.Users)
        {
            if(Guid.Parse(user.Uid) == _authManager.UserGuid)
            {
                continue;
            }

            var newUser = CreateSyncObject(user, Vector3.up);
            if(newUser is null)
            {
                Debug.LogError($"{user.Username} is already exist");
            }
        }
    }

    private GameObject CreateSyncObject(UserSimpleDto user, Vector3 position)
    {
        GameObject syncObject = Instantiate(_syncObjectPrefab, position, Quaternion.identity); // Create the object

        syncObject.transform.SetParent(transform); // Set parent to SyncManager
        syncObject.name = user.Username; // Set the name to the objectId
        syncObject.GetComponent<SyncObject>().Init(user); // Initialize SyncObject

        // Create the name tag
        GameObject nameTag = Instantiate(_syncObjectNameTagPrefab, _syncObjectCanvas);
        nameTag.GetComponent<NameTagController>().Init(user, syncObject);

        var userId = Guid.Parse(user.Uid);
        if (!_syncObjects.TryAdd(userId, syncObject)) // Add to dict
        {
            Destroy(nameTag);
            Destroy(syncObject);
            return null;
        }

        if(!_userInfos.TryAdd(userId, user))
        {
            Destroy(nameTag);
            Destroy(syncObject);
            return null;
        }

        return syncObject;
    }

    public void SyncObjectPosition(Guid userId, MoveData moveData)
    {
        if (isManualMode)
        {
            LogManager.Instance.Log($"{userId} : {moveData.X}, {moveData.Y}, {moveData.Z}, Speed: {moveData.Speed}");
            if (_manualConnector.UserId == userId)
                return;

            if (userId == Guid.Empty)
            {
                Debug.Log($"Empty ObjectId received in SyncObjectPosition. Ignoring.");
                _connector.IncrementErrorCount();
                return;
            }

            if (!_userInfos.TryGetValue(userId, out var manualUser))
            {
                Debug.LogError($"Invalid Situation: {userId} is not exist in syncObjects");
                return;
            }

            var manualStartPosition = new Vector3(moveData.X, moveData.Y, moveData.Z);
            if (!_syncObjects.TryGetValue(userId, out var manualSyncObject))
            {
                var newSyncObject = CreateSyncObject(manualUser, manualStartPosition);
                _syncObjects.Add(userId, newSyncObject);
                manualSyncObject = newSyncObject;
            }

            var manualSyncObjectComponent = manualSyncObject.GetComponent<SyncObject>();
            manualSyncObjectComponent.EnqueueMoveData(moveData);
            return;
        }

        LogManager.Instance.Log($"{userId} : {moveData.X}, {moveData.Y}, {moveData.Z}, Speed: {moveData.Speed}");
        if (userId == _authManager.UserGuid)
            return;

        Debug.Log($"Received SyncObjectPosition - {userId} : {moveData.X}, {moveData.Y}, {moveData.Z}, Speed: {moveData.Speed}");

        if (userId == Guid.Empty)
        {
            Debug.Log($"Empty ObjectId received in SyncObjectPosition. Ignoring.");
            _connector.IncrementErrorCount();
            return;
        }

        if(!_userInfos.TryGetValue(userId, out var user))
        {
            Debug.LogError($"Invalid Situation: {userId} is not exist in syncObjects");
            return;
        }

        var startPosition = new Vector3(moveData.X, moveData.Y, moveData.Z);
        if (!_syncObjects.TryGetValue(userId, out var syncObject))
        {
            var newSyncObject = CreateSyncObject(user, startPosition);
            _syncObjects.Add(userId, newSyncObject);
            syncObject = newSyncObject;
        }

        var syncObjectComponent = syncObject.GetComponent<SyncObject>();
        syncObjectComponent.EnqueueMoveData(moveData);
    }

    public void TestAttackProcess(Guid userId, AtkData atkData)
    {
        string hitUser = string.IsNullOrEmpty(atkData.To) ? "none" : atkData.To;

        LogManager.Instance.Log($"{userId} : attack {hitUser}, damage {atkData.Dmg}");
    }

    public void SyncObjectNone(Guid objectId)
    {
        // self packet check and return
        if (objectId == _authManager.UserGuid)
            return;

        Debug.Log($"SyncObjectNone - {objectId}");
    }

    private void CreateTestSyncObject()
    {
        const int MAX_USER = 4;

        for (int i = 0; i < MAX_USER; ++i)
        {
            GameObject randomObject = Instantiate(_syncObjectPrefab, Vector3.up, Quaternion.identity);
            randomObject.transform.SetParent(transform);
            randomObject.name = $"user {i + 1}";
            randomObject.GetComponent<SyncObject>().Init(null);

            randomObject.transform.position = new Vector3(Random.Range(0, 10), 1, Random.Range(0, 10));

            GameObject nameTag = Instantiate(_syncObjectNameTagPrefab, _syncObjectCanvas);
            nameTag.GetComponent<NameTagController>().Init(randomObject.name, randomObject);

            _syncObjects.Add(Guid.NewGuid(), randomObject);
        }
    }
}