using System;
using System.Collections;
using System.Collections.Generic;
using Network;
using NetworkData;
using UnityEngine;

[RequireComponent(typeof(MeshRenderer))]
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
    private readonly Queue<AtkData> _atkQueue = new();

    private IEnumerator _syncPositionRoutine = null;
    private IEnumerator _syncAttackRoutine = null;

    private GameObject _sword;

    private bool IsOwnObject 
    { 
        get 
        {
            if (_isManualMode)
                return Guid.Parse(_user.Uid) == ManualConnector.Instance.UserId;
            return Guid.Parse(_user.Uid) == _authManager.UserGuid; 
        } 
    }

    private bool _isManualMode = false;

    private void Awake()
    {
        _sword = transform.GetChild(0).gameObject;
    }

    public void Init(UserSimpleDto user)
    {
        if(user is null)
        {
            _isManualMode = true;
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
        _isManualMode = true;

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

    public void EnqueueAtkData(AtkData atkData)
    {
        _atkQueue.Enqueue(atkData);
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
        if (_syncPositionRoutine is null)
        {
            _syncPositionRoutine = ManualSyncPosition();
            StartCoroutine(_syncPositionRoutine);
        }

        if(_syncAttackRoutine is null)
        {
            _syncAttackRoutine = ManualSyncAttack();
            StartCoroutine(_syncAttackRoutine);
        }
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
            Vector3 newPosition = new(nextData.X, nextData.Y, nextData.Z);
            transform.position = Vector3.Lerp(transform.position, newPosition, 1.0f);
            yield return null;
        }

        _syncPositionRoutine = null;
    }

    public IEnumerator ManualSyncAttack()
    {
        if (!ManualConnector.Instance.IsOnline)
            yield break;

        if(_atkQueue.Count == 0)
        {
            _syncAttackRoutine = null;
            yield break;
        }

        var atkData = _atkQueue.Dequeue(); // 이건 나중에 활용 (보여지는 부분)
        const float duration = 0.5f;
        float delta = 0.0f;

        Vector3 originalPosition = _sword.transform.localPosition;
        Vector3 lungeOffset = Vector3.back;

        while (delta < duration)
        {
            float t = delta / duration;
            float curve = Mathf.Sin(t * Mathf.PI);

            _sword.transform.localPosition = originalPosition + lungeOffset * curve;

            yield return null;
            delta += Time.deltaTime;
        }

        _sword.transform.localPosition = originalPosition;
        _syncAttackRoutine = null;
    }


    private void Update()
    {
        if (_isManualMode)
        {
            if (IsOwnObject)
                _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);

            Sync();
            return;
        }

        if (!_connector.IsOnline)
            return;

        if (IsOwnObject)
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
    }
}