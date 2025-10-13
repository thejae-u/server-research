using Network;
using UnityEngine;
using TMPro;

public class ErrorRateTextController : MonoBehaviour
{
    private LogicServerConnector _networkManager;
    private TMP_Text _errorRateText;

    private void Awake()
    {
        _errorRateText = GetComponent<TMP_Text>();
    }

    private void Start()
    {
        _networkManager = LogicServerConnector.Instance;
    }

    private void Update()
    {
        _errorRateText.text = $"Error Rate : {_networkManager.ErrorRate:F2}%";
    }
}
