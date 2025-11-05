using System;
using UnityEngine;
using UnityEngine.InputSystem;

[CreateAssetMenu(fileName = "PlayerMoveActions", menuName = "Scriptable Objects/PlayerMoveActions")]
public class PlayerMoveActions : ScriptableObject, IA_Base.IPlayerActions
{
    public Action onMoveStartAction;
    public Action<Vector2> onMoveAction;
    public Action onMoveStopAction;
    
    private static IA_Base _iaBase;
    private void OnEnable()
    {
        if (_iaBase == null)
        {
            _iaBase = new IA_Base();
            _iaBase.Player.SetCallbacks(this);
        }

        _iaBase.Player.Enable();
    }
    
    private void OnDisable()
    {
        _iaBase.Player.Disable();

        if (_iaBase == null) return;
        _iaBase.Player.SetCallbacks(null);
        _iaBase = null;
    }

    public void OnMove(InputAction.CallbackContext context)
    {
        if (context.started)
        {
            var input = context.ReadValue<Vector2>();
            onMoveAction?.Invoke(input);
            return;
        }
        
        if (context.canceled)
        {
            onMoveStopAction?.Invoke();
            return;
        }
        
        if (!context.performed)
            return;
        
        //var input = context.ReadValue<Vector2>();
        //onMoveAction?.Invoke(input);
    }
}
