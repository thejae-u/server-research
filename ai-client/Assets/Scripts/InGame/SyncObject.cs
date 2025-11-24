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

    private BaseConnector _connector;
    private MeshRenderer _meshRenderer;

    private bool _isMoving = false;
    private Vector3 _direction = Vector3.zero;

    private readonly Queue<AtkData> _atkQueue = new();

    private IEnumerator _syncAttackRoutine = null;

    private GameObject _sword;

    private bool IsOwnObject => Guid.Parse(_user.Uid) == _connector.UserId;

    private void Awake()
    {
        _sword = transform.GetChild(0).gameObject;
    }

    public void Init(UserSimpleDto user, BaseConnector connector)
    {
        _user = user;
        _connector = connector;
        _meshRenderer = GetComponent<MeshRenderer>();

        if (IsOwnObject)
        {
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);
        }
    }

    public void SetMovementState(RpcMethod method, MoveData moveData)
    {
        transform.position = new Vector3(moveData.X, moveData.Y, moveData.Z);
        _direction = new Vector3(moveData.Horizontal, 0, moveData.Vertical);

        switch (method)
        {
            case RpcMethod.MoveStart:
                _isMoving = true;
                break;
            case RpcMethod.Move:
                _isMoving = true;
                break;
            case RpcMethod.MoveStop:
                _isMoving = false;
                break;
        }
    }

    public void EnqueueAtkData(AtkData atkData)
    {
        _atkQueue.Enqueue(atkData);
    }
    
    public IEnumerator SyncAttackRoutine()
    {
        if (!_connector.IsOnline)
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
        if (_connector == null || !_connector.IsOnline)
            return;

        if (IsOwnObject)
            _meshRenderer.material.color = new Color(0, 1, 0, 0.5f);

        if (_isMoving)
        {
            transform.position += _direction * (_playerStatData.speed * Time.deltaTime);
        }
        
        if(_syncAttackRoutine is null && _atkQueue.Count > 0)
        {
            _syncAttackRoutine = SyncAttackRoutine();
            StartCoroutine(_syncAttackRoutine);
        }
    }
}