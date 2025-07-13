using UnityEngine;

public class ErrorRateTextController : MonoBehaviour
{
    private NetworkManager _networkManager;
    private TMPro.TMP_Text _errorRateText;

    private void Awake()
    {
        _errorRateText = GetComponent<TMPro.TMP_Text>();
    }

    private void Start()
    {
        _networkManager = NetworkManager.Instance;
    }

    private void Update()
    {
        _errorRateText.text = $"Error Rate : {_networkManager.ErrorRate:F2}%";
    }
}
