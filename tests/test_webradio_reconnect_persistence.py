from pathlib import Path

SRC = Path("src/services/RadioService.cpp").read_text(encoding="utf-8")

def test_connect_fail_schedules_retry_not_error():
    assert 'scheduleRetry("connect_failed", 1500U)' in SRC
    assert 'setState(ERROR_STATE, "connect_failed")' not in SRC

def test_wifi_loss_schedules_retry():
    assert 'scheduleRetry("wifi_not_connected", 3000U)' in SRC

def test_retry_backoff_is_bounded():
    assert 'min<uint32_t>(15000U, backoff)' in SRC
