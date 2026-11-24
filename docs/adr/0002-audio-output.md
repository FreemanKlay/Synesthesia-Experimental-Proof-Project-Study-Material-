# ADR-0002: Saída de áudio (WASAPI) e fila SPSC

**Status:** aceita (Fase 1)

## Decisão
- WASAPI em **modo compartilhado, orientado a evento**, formato float32 estéreo a 44,1 kHz com
  `AUTOCONVERTPCM | SRC_DEFAULT_QUALITY` (o Windows reamostra se o dispositivo usar outra taxa).
- O callback roda numa thread própria com MMCSS ("Pro Audio"). Ela **só consome** da `SpscQueue`
  (`pop_n`), sem alocação nem locks. Fila seca → silêncio + contador de underrun.
- O produtor (futuramente a thread de física) mantém a fila em ~93 ms de folga.
- Métricas coletadas na própria thread de áudio com atômicos relaxed e um array pré-alocado:
  underruns, frames de silêncio, wakeups atrasados, maior intervalo, tempo do callback (percentis).

## Latência
Latência total ≈ folga da fila + latência do stream (`GetStreamLatency`). O sandbox imprime ambas.
Reduzir a folga e usar `IAudioClient3` (período mínimo) ou modo exclusivo são alavancas futuras.

## Limitações conhecidas
Modo compartilhado tem piso de latência ~10 ms (período padrão). Linux (ALSA/PipeWire) fica para depois.
