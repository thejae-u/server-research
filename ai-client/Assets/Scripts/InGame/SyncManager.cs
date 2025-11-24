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

    private BaseConnector _connector;
    private AuthManager _authManager;

    private readonly Dictionary<Guid, GameObject> _syncObjects = new();
    private readonly Dictionary<Guid, UserSimpleDto> _userInfos = new();

    private readonly Queue<Tuple<Guid, RpcMethod, MoveData>> _moveDataQueue = new();
    private readonly Queue<Tuple<Guid, AtkData>> _atkDataQueue = new();

    private void Awake()
    {
        isManualMode = true;
    }

    private void Start()
    {
        if(isManualMode)
        {
            _connector = ManualConnector.Instance;
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

    private void Update()
    {
        SyncObjectPosition();
        SyncAttack();
    }

    public void Enqueue(Guid userId, RpcMethod method, MoveData moveData)
    {
        _moveDataQueue.Enqueue(new Tuple<Guid, RpcMethod, MoveData>(userId, method, moveData));
    }

    private GameObject CreateSyncObject(UserSimpleDto user, Vector3 position)
    {
        GameObject syncObject = Instantiate(_syncObjectPrefab, position, Quaternion.identity); // Create the object

        syncObject.transform.SetParent(transform); // Set parent to SyncManager
        syncObject.name = user.Username; // Set the name to the objectId

        syncObject.GetComponent<SyncObject>().Init(user, _connector); // Initialize SyncObject

        // Create the name tag
        GameObject nameTag = Instantiate(_syncObjectNameTagPrefab, _syncObjectCanvas);
        nameTag.GetComponent<NameTagController>().Init(user, syncObject, isManualMode);

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

    private void SyncObjectPosition()
    {
        if(!_moveDataQueue.TryDequeue(out var data))
        {
            return;
        }

        var userId = data.Item1;
        var method = data.Item2;
        var moveData = data.Item3;

        if (isManualMode)
        {
            LogManager.Instance.Log($"{userId} : {moveData.X}, {moveData.Y}, {moveData.Z}, Speed: {moveData.Speed}");
            if (userId == Guid.Empty)
            {
                Debug.Log($"Empty ObjectId received in SyncObjectPosition. Ignoring.");
                _connector.IncrementErrorCount();
                return;
            }

            var movePosition = new Vector3(moveData.X, moveData.Y, moveData.Z);
            var manualSyncObjectComponent = GetValidObject(userId, movePosition);
            manualSyncObjectComponent.SetMovementState(method, moveData);
            return;
        }

        // ---------------------------------------------------------------------------------------------------------

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

        var startPosition = new Vector3(moveData.X, moveData.Y, moveData.Z);
        var syncObjectComponent = GetValidObject(userId, startPosition);
        syncObjectComponent.SetMovementState(method, moveData);
    }

    private void SyncAttack()
    {
        if(!_atkDataQueue.TryDequeue(out var result))
        {
            return;
        }

        var uid = result.Item1;
        var atkData = result.Item2;

        var syncObject = GetValidObject(uid, Vector3.zero);
        syncObject.EnqueueAtkData(atkData);
    }

    public void EnqueueAttackData(Guid userId, AtkData atkData)
    {
        _atkDataQueue.Enqueue(new Tuple<Guid, AtkData>(userId, atkData));

        string hitUser = string.IsNullOrEmpty(atkData.To) ? "none" : atkData.To;
        LogManager.Instance.Log($"{userId} : attack {hitUser}, damage {atkData.Dmg}");
    }

    private SyncObject GetValidObject(Guid userId, Vector3 initPos)
    {
        if (!_syncObjects.TryGetValue(userId, out var syncObject))
        {
            UserSimpleDto newUserDto = new()
            {
                Uid = userId.ToString(),
                Username = $"test{_userInfos.Count}"
            };

            var newSyncObject = CreateSyncObject(newUserDto, initPos);
            syncObject = newSyncObject;
        }

        return syncObject.GetComponent<SyncObject>();
    }

    public void SyncObjectNone(Guid objectId)
    {
        // self packet check and return
        if (objectId == _authManager.UserGuid)
            return;

        Debug.Log($"SyncObjectNone - {objectId}");
    }
}