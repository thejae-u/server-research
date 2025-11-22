using System;
using System.Collections;
using System.Collections.Generic;
using Network;
using NetworkData;
using UnityEngine;

public class SyncObject : MonoBehaviour
{
    [SerializeField] private PlayerStatData _playerStatData; // Player Stat Data for the object

    // Several Sync Data can be added to the SyncObject
    public UserSimpleDto _user { get; private set; }

    private LogicServerConnector _connector;
    private AuthManager _authManager;
    private MeshRenderer _meshRenderer;

    private Vector3 _lastNetworkPosition;
    private readonly Queue<MoveData> _moveQueue = new();
    private IEnumerator _syncPositionRoutine = null;

    private bool _isMyObject => Guid.Parse(_user.Uid) == _authManager.UserGuid;
    private bool _isMannualMode = false;

    public void Init(UserSimpleDto user)
    {
        if(user is null)
        {
            _isMannualMode = true;
            return;
        }

        _user = user;
        _meshRenderer = GetComponent<MeshRenderer>();

        _connector = LogicServerConnector.Instance;
        _authManager = AuthManager.Instance;

        if (_connector is null || _authManager is null)
            return;

        if (Guid.Parse(_user.Uid) == _authManager.UserGuid)
        {
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
        }

        _syncPositionRoutine = SyncPositionRoutine();
        StartCoroutine(_syncPositionRoutine);
    }

    public void ManualModeInit(UserSimpleDto user)
    {
        _isMannualMode = true;

        _user = user;
        _meshRenderer = GetComponent<MeshRenderer>();

        if(Guid.Parse(_user.Uid) == ManualConnector.Instance.UserId)
        {
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
        }

        _syncPositionRoutine = null;
    }

    public void EnqueueMoveData(MoveData moveData)
    {
        _moveQueue.Enqueue(moveData);
    }

    private IEnumerator SyncPositionRoutine()
    {
        while (_connector.IsOnline)
        {
            if (_moveQueue.Count == 0)
                continue;

            MoveData nextData = _moveQueue.Dequeue();
            yield return null;
        }

        _syncPositionRoutine = null;
    }

    public void Sync()
    {
        if (_syncPositionRoutine is not null)
        {
            return;
        }

        _syncPositionRoutine = ManualSyncPosition();
        StartCoroutine(_syncPositionRoutine);
    }

    public IEnumerator ManualSyncPosition()
    {
        if(ManualConnector.Instance.IsOnline)
        {
            if (_moveQueue.Count == 0)
            {
                _syncPositionRoutine = null;
                yield break;
            }

            MoveData nextData = _moveQueue.Dequeue();
            LogManager.Instance.Log($"{_user.Uid} next Data : {nextData.X},{nextData.Y},{nextData.Z}");
            transform.position = new Vector3(nextData.X, nextData.Y, nextData.Z);
            yield return null;
        }

        _syncPositionRoutine = null;
    }


    private void Update()
    {
        if (_isMannualMode)
        {
            Sync();
            return;
        }

        if (!_connector.IsOnline)
            return;

        if (_isMyObject)
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
    }
}