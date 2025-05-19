using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using UnityEngine;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using NetworkData;
using Random = UnityEngine.Random;

public class AIManager : MonoBehaviour
{
    private NetworkManager _networkManager;
    
    private void Start()
    {
        _networkManager = NetworkManager.Instance;
    }
    
    public void MoveTo(Vector3 targetPosition)
    {
        Vector3 startPosition = transform.position;
        transform.position = Vector3.Lerp(startPosition, targetPosition, 0.1f);
    }
}