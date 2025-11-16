using System;
using System.Collections;
using UnityEngine;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Network;
using NetworkData;

public class OwnObjectManager : MonoBehaviour
{
    [SerializeField] private PlayerMoveActions _playerActions;
    [SerializeField] private PlayerStatData _playerStatData;
    [SerializeField] private float _movePacketSendInterval = 16.6f; // in milliseconds

    private GameObject _sword;
    private bool IsOnAttack => _attackMotionRoutine is not null;
    private IEnumerator _attackMotionRoutine;

    private LogicServerConnector _connector;

    private bool _isMoving;

    private Vector3 _cachedDirection;
    private float _lastSendDeltaTime = 0.0f;
    private MoveData _moveData = new()
    {
        X = 0.0f,
        Y = 0.0f,
        Z = 0.0f,
        Horizontal = 0.0f,
        Vertical = 0.0f,
        Speed = 0.0f
    };

    private readonly RpcPacket _sendPacket = new();

    private bool _isManualMode = false;

    private void Awake()
    {
        _sword = transform.GetChild(0).gameObject;
    }

    private void Start()
    {
        if (SyncManager.Instance.isManualMode)
        {
            _isManualMode = true;
            _isMoving = false;
            return;
        }

        _connector = LogicServerConnector.Instance;
        _connector.StartGameTask();
        _isMoving = false;
    }

    private void OnEnable()
    {
        _playerActions.onMoveStartAction += OnMoveStart;
        _playerActions.onMoveAction += OnMove;
        _playerActions.onMoveStopAction += OnMoveStop;

        _playerActions.onAttackAction += OnAttack;
    }

    private void OnDisable()
    {
        _playerActions.onMoveAction -= OnMove;
        _playerActions.onMoveStopAction -= OnMoveStop;
        _playerActions.onMoveStartAction -= OnMoveStart;

        _playerActions.onAttackAction -= OnAttack;
    }

    private void Update()
    {
        if (_isManualMode)
        {
            if (!ManualConnector.Instance.IsOnline)
                return;

            _lastSendDeltaTime += Time.deltaTime * 1000.0f; // Convert to milliseconds

            if (!_isMoving)
                return;

            MoveTo();
            return;
        }

        if (!_connector.IsOnline)
            return;

        _lastSendDeltaTime += Time.deltaTime * 1000.0f; // Convert to milliseconds

        if (!_isMoving)
            return;

        MoveTo();
    }

    private void SendMovementPacket(RpcMethod method, bool isForce = false)
    {
        if (!isForce)
        {
            if (_lastSendDeltaTime < _movePacketSendInterval)
            {
                return;
            }
        }

        _lastSendDeltaTime = 0.0f;

        _moveData.X = transform.position.x;
        _moveData.Y = transform.position.y;
        _moveData.Z = transform.position.z;

        _sendPacket.Method = method;
        _sendPacket.Timestamp = Timestamp.FromDateTime(DateTime.UtcNow);
        _sendPacket.Data = _moveData.ToByteString();

        if (_isManualMode)
            ManualConnector.Instance.EnqueueRpcPacketForUdp(_sendPacket);
        else
            _connector.EnqueueRpcPacketForUdp(_sendPacket);
    }

    public void MoveTo()
    {
        if (_cachedDirection == Vector3.zero) return;
        transform.position += _cachedDirection * (_playerStatData.speed * Time.deltaTime);
    }

    private void OnMoveStart()
    {
        _isMoving = true;
        SendMovementPacket(RpcMethod.MoveStart, true);
    }

    private void OnMove(Vector2 move)
    {
        Vector3 dir = transform.right * move.x + transform.forward * move.y;
        _cachedDirection = dir.sqrMagnitude > 0.0001f ? dir.normalized : Vector3.zero;

        _moveData.Horizontal = dir.x;
        _moveData.Vertical = dir.z;
        _moveData.Speed = _playerStatData.speed;
        SendMovementPacket(RpcMethod.Move);
    }

    private void OnMoveStop()
    {
        _isMoving = false;
        SendMovementPacket(RpcMethod.MoveStop, true);
    }

    private void OnAttack()
    {
        if (IsOnAttack)
            return;

        RpcPacket attackPacket = new()
        {
            Method = RpcMethod.Atk,
            Data = ByteString.Empty,
            Timestamp = Timestamp.FromDateTime(DateTime.UtcNow)
        };

        if (_isManualMode)
            ManualConnector.Instance.EnqueueRpcPacketForUdp(attackPacket);
        else
            _connector.EnqueueRpcPacketForUdp(attackPacket);
        PlayAttackMotion();
    }

    private void PlayAttackMotion()
    {
        if (_attackMotionRoutine is not null)
            return;

        _attackMotionRoutine = AttackMotionRoutine();
        StartCoroutine(_attackMotionRoutine);
    }

    private IEnumerator AttackMotionRoutine()
    {
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
        _attackMotionRoutine = null;
    }
}