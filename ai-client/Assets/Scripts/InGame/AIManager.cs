using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using Cysharp.Threading.Tasks;
using UnityEngine;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using Network;
using NetworkData;
using Random = UnityEngine.Random;

public class AIManager : MonoBehaviour
{
    [SerializeField] private PlayerMoveActions _playerMoveActions;
    [SerializeField] private PlayerStatData _playerStatData;
    [SerializeField] private float _movePacketSendInterval = 16.6f; // in milliseconds

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

    private void Start()
    {
        _connector = LogicServerConnector.Instance;
        _isMoving = false;
    }

    private void OnEnable()
    {
        _playerMoveActions.onMoveStartAction += OnMoveStart;
        _playerMoveActions.onMoveAction += OnMove;
        _playerMoveActions.onMoveStopAction += OnMoveStop;
    }

    private void OnDisable()
    {
        _playerMoveActions.onMoveAction -= OnMove;
        _playerMoveActions.onMoveStopAction -= OnMoveStop;
        _playerMoveActions.onMoveStartAction -= OnMoveStart;
    }

    private void Update()
    {
        if (!_connector.IsOnline)
            return;

        _lastSendDeltaTime += Time.deltaTime * 1000.0f; // Convert to milliseconds

        if (!_isMoving)
        {
            return;
        }

        MoveTo();
        SendPacket();
    }

    private void SendPacket()
    {
        if (_lastSendDeltaTime < _movePacketSendInterval)
        {
            return;
        }

        _lastSendDeltaTime = 0.0f;

        _moveData.X = transform.position.x;
        _moveData.Y = transform.position.y;
        _moveData.Z = transform.position.z;

        _sendPacket.Timestamp = Timestamp.FromDateTime(DateTime.UtcNow);
        _sendPacket.Data = _moveData.ToByteString();

        var task = _connector.AsyncWriteRpcPacketByUdp(_sendPacket, _connector.CToken);
    }

    public void MoveTo()
    {
        if (_cachedDirection == Vector3.zero) return;
        transform.position += _cachedDirection * (_playerStatData.speed * Time.deltaTime);
    }

    private void OnMoveStart()
    {
        _isMoving = true;

        _sendPacket.Method = RpcMethod.MoveStart;
        _moveData.Horizontal = 0;
        _moveData.Vertical = 0;
        _moveData.Speed = 0;
    }

    private void OnMove(Vector2 move)
    {
        Vector3 dir = transform.right * move.x + transform.forward * move.y;
        _cachedDirection = dir.sqrMagnitude > 0.0001f ? dir.normalized : Vector3.zero;

        _sendPacket.Method = RpcMethod.Move;
        _moveData.Horizontal = dir.x;
        _moveData.Vertical = dir.z;
        _moveData.Speed = _playerStatData.speed;
    }

    private void OnMoveStop()
    {
        _isMoving = false;

        _sendPacket.Method = RpcMethod.MoveStop;
        _moveData.Horizontal = 0;
        _moveData.Vertical = 0;
        _moveData.Speed = 0;
    }
}