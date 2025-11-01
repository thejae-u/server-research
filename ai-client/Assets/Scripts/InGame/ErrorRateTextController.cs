using Network;
using UnityEngine;
using TMPro;

public class ErrorRateTextController : MonoBehaviour
{
    private LogicServerConnector _networkManager;
    private TMP_Text _errorRateText;
    private bool _isMannualMode = false;

    private void Awake()
    {
        _errorRateText = GetComponent<TMP_Text>();
    }

    private void Start()
    {
        if(SyncManager.Instance.isManualMode)
        {
            _errorRateText.text = $"Error Rate : Mannual Mode";
            _isMannualMode = true;
            return;
        }

        _networkManager = LogicServerConnector.Instance;
    }

    private void Update()
    {
        if (_isMannualMode)
            return;

        _errorRateText.text = $"Error Rate : {_networkManager.ErrorRate:F2}%";
    }
}
