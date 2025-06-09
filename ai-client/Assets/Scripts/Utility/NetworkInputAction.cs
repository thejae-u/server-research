using System;
using UnityEngine;
using UnityEngine.InputSystem;

[CreateAssetMenu(fileName = "NetworkInputAction", menuName = "Scriptable Objects/NetworkInputAction")]
public class NetworkInputAction : ScriptableObject, IA_Base.INetworkActions
{
    private static IA_Base _iaBase;
    
    public Action connectAction;
    public Action disconnectAction;
    public Action sendPacketToggleAction;

    private void OnEnable()
    {
        if (_iaBase == null)
        {
            _iaBase = new IA_Base();
            _iaBase.Network.SetCallbacks(this);
        }

        _iaBase.Network.Enable();
    }
    
    private void OnDisable()
    {
        _iaBase.Network.Disable();

        if (_iaBase == null) return;
        _iaBase.Network.SetCallbacks(null);
        _iaBase = null;
    }
    
    public void OnConnectToServer(InputAction.CallbackContext ctx)
    {
        if (!ctx.started)
        {
            return;
        }
        
        Debug.Log($"Clicked connect button");
        
        // Handle the action when the button is pressed
        connectAction?.Invoke();
    }
    
    public void OnDisconnect(InputAction.CallbackContext ctx)
    {
        if (!ctx.started)
        {
            return;
        }
        
        Debug.Log($"Clicked disconnect button");
        
        // Handle the action when the button is pressed
        disconnectAction?.Invoke();
    }

    public void OnSendPacketToggle(InputAction.CallbackContext ctx)
    {
        if (!ctx.started)
            return;
        
        Debug.Log($"Clicked send packet toggle button");
        sendPacketToggleAction?.Invoke();       
    }
}