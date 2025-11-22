using UnityEngine;
using System.Collections.Generic;
using System.Text;
using System;
using TMPro;      // TextMeshPro 사용
using UnityEngine.UI; // ScrollRect 사용

// 로그 타입에 따라 스타일을 구분하기 위한 열거형
public enum LogType
{
    Normal,
    Warning,
    Error,
    System // 연결/단절 등 시스템 메시지
}

public class LogManager : MonoBehaviour
{
    // === UI 요소 참조 (Inspector에서 연결 필요) ===
    [Header("UI References")]
    [SerializeField] private TMP_Text logTextComponent; // 로그 내용을 표시할 TextMeshPro 컴포넌트
    [SerializeField] private ScrollRect scrollRect;     // 스크롤을 맨 아래로 자동 이동시키기 위한 ScrollRect 컴포넌트

    // === 로그 관리 변수 ===
    private List<string> _logs = new List<string>();
    private StringBuilder _logBuilder = new StringBuilder();

    [Header("Settings")]
    [SerializeField] private int _maxLogLine = 50; // 최대 로그 라인 수 (0이면 무제한)

    // === 단일 씬 접근을 위한 정적 인스턴스 ===
    private static LogManager _instance;
    public static LogManager Instance => _instance;

    private void Awake()
    {
        // 씬 내에서 인스턴스를 설정합니다.
        _instance = this;
    }

    private void OnDestroy()
    {
        // 오브젝트 파괴 시 정적 참조를 해제합니다.
        if (_instance == this)
        {
            _instance = null;
        }
    }

    /// <summary>
    /// 로그 메시지를 추가하고 UI를 업데이트합니다.
    /// </summary>
    /// <param name="message">기록할 메시지 내용</param>
    /// <param name="type">로그 타입 (색상 결정)</param>
    public void Log(string message, LogType type = LogType.System)
    {
        if (logTextComponent == null)
        {
            // UI 연결이 없으면 일반 Debug.Log로 대체 (선택 사항)
            Debug.Log($"[{type}] {message}");
            return;
        }

        string timestamp = $"[{DateTime.UtcNow.ToLocalTime():HH:mm:ss}]";
        // Rich Text 태그를 사용하여 스타일링된 메시지를 생성
        string styledMessage = ApplyRichTextTag(timestamp, message, type);

        // 1. 데이터 누적
        _logs.Add(styledMessage);

        // 2. 최대 라인 수 초과 시 가장 오래된 로그(맨 앞) 제거
        if (_maxLogLine > 0 && _logs.Count > _maxLogLine)
        {
            _logs.RemoveAt(0);
        }

        // 3. StringBuilder를 사용하여 로그 내용을 재구성
        // (주의: 로그가 빈번할 경우 이 부분은 별도의 업데이트 주기를 갖는 것이 좋습니다.)
        _logBuilder.Clear();
        foreach (string log in _logs)
        {
            _logBuilder.AppendLine(log);
        }

        // 4. TextMeshPro UI 업데이트
        logTextComponent.text = _logBuilder.ToString();

        // 5. 스크롤을 맨 아래 (최신 로그)로 이동
        if (scrollRect != null)
        {
            // 레이아웃 업데이트를 강제하여 스크롤 위치를 정확히 잡습니다.
            Canvas.ForceUpdateCanvases();
            scrollRect.verticalNormalizedPosition = 0f; // 0은 맨 아래를 의미
        }
    }

    /// <summary>
    /// 로그 타입에 따라 Rich Text 태그를 적용하여 스타일을 결정합니다.
    /// </summary>
    private string ApplyRichTextTag(string timestamp, string message, LogType type)
    {
        string colorTagStart;
        string colorTagEnd = "</color>";

        // 로그 타입에 따라 색상과 스타일을 결정합니다.
        switch (type)
        {
            case LogType.Normal:
                // 일반 메시지: 밝은 회색
                colorTagStart = "<color=#00FF00><b>";
                break;
            case LogType.Warning:
                // 경고 메시지: 노란색 볼드체
                colorTagStart = "<color=#FFFF00><b>";
                message += "</b>"; // 메시지 끝에 볼드체 태그 닫기
                break;
            case LogType.Error:
                // 에러 메시지: 빨간색
                colorTagStart = "<color=#FF0000>";
                break;
            case LogType.System:
                // 시스템 메시지: 녹색 볼드체
                colorTagStart = "<color=#000000>";
                message += "</b>";
                break;
            default:
                colorTagStart = "<color=#CCCCCC>";
                break;
        }

        // [시간] 메시지 형식으로 태그를 적용하여 최종 문자열 반환
        return $"{colorTagStart}{timestamp} {message}{colorTagEnd}";
    }
}