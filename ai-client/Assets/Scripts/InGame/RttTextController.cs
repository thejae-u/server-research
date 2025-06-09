using TMPro;
using UnityEngine;

public class RttTextController : MonoBehaviour
{
    private NetworkManager _networkManager;
    private TMP_Text _rttText;

    private void Awake()
    {
        _rttText = GetComponent<TMP_Text>();
    }

    private void Start()
    {
        _networkManager = NetworkManager.Instance;
    }

    private void Update()
    {
        _rttText.text = $"Network : {_networkManager.LastRtt.ToString()}ms";
    }
}
