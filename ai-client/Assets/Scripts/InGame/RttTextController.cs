using Network;
using TMPro;
using UnityEngine;

public class RttTextController : MonoBehaviour
{
    private LogicServerConnector _networkManager;
    private TMP_Text _rttText;
    private TMP_Text _averageRttText;
    private bool _isMannualMode = false;

    private void Awake()
    {
        _rttText = GetComponent<TMP_Text>();
        _averageRttText = transform.GetChild(0).GetComponent<TMP_Text>();
    }

    private void Start()
    {
        if (SyncManager.Instance.isManualMode)
        {
            _rttText.text = $"Network : Mannual Mode";
            _averageRttText.text = $"Average RTT : Mannual Mode";
            _isMannualMode = true;
            return;
        }

        _networkManager = LogicServerConnector.Instance;
    }

    private void Update()
    {
        if (_isMannualMode)
            return;

        _rttText.text = $"Network : {_networkManager.LastRtt.ToString()}ms";
        _averageRttText.text = $"Average RTT : {_networkManager.RttAverage.ToString()}ms";
    }
}
