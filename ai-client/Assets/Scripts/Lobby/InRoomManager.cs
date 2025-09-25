using System;
using System.Collections;
using UnityEngine;
using System.Collections.Generic;
using UnityEngine.UI;
using Utility;

public class InRoomManager : MonoBehaviour
{
    private readonly List<GameObject> _players = new(4);
    
    [SerializeField] private Button _startButton;

    private IEnumerator _roomUpdateRoutine;

    private AuthManager _authManager;

    private void Awake()
    {
        _authManager = AuthManager.Instance;
        
        for (var i = 0; i < 4; ++i)
        {
            _players.Add(transform.GetChild(0).GetChild(i).gameObject);
        }
    }
    
    private void OnEnable()
    {
        _roomUpdateRoutine = RoomUpdateRoutine();
        StartCoroutine(_roomUpdateRoutine);
    }

    private void OnDisable()
    {
        if (_roomUpdateRoutine is null) return;
        StopCoroutine(_roomUpdateRoutine);
        _roomUpdateRoutine = null;
    }

    private IEnumerator RoomUpdateRoutine()
    {
        while (true)
        {
            yield return null;
        }
    }
}
