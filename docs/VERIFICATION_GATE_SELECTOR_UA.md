# Sparse Verification/Gate Keyframe Selector

## Статус

`VerificationKeyframeSelector` — ізольований library-only policy engine. Він визначає, коли варто запросити native-resolution verification кадр і чи може цей кадр бути gate-кандидатом. Модуль не відкриває камеру, не записує `1280x800`, не змінює `VHRM`, не виконує global search, не створює `reset_reference` і не має FC/UART/ODOMETRY/flight authority.

## Двофазний Контракт

```text
low-resolution observation + local/route metadata
        -> evaluate()
        -> request_native_capture / gate_candidate / reasons
        -> external native capture + durable package write
        -> commit() only after success
```

`evaluate()` не змінює стан. Повторна оцінка того самого observation повертає рішення для тієї самої generation. `commit()` повторно обчислює очікуване рішення і відхиляє stale, forged або невідповідний frame/timestamp. Тому невдале захоплення чи запис великого кадру не пересуває selection reference і не створює уявний gate. `reset()` очищує прийняті references/counters та інвалідовує всі попередні рішення.

## Verification Triggers

Після першого обов'язкового кадру запит формується, якщо після minimum interval виконався хоча б один поріг:

- displacement: 3D local distance, якщо обидві точки мають local pose; інакше absolute route-progress delta, помножена на nominal route length;
- relative-altitude change;
- absolute logarithmic scale-ratio change;
- wrapped yaw change у межах `0..pi`;
- scene novelty як normalized mean absolute difference supplied signed-int8 descriptors;
- maximum interval як starvation guard.

Default policy: не частіше `1 s`, примусово не рідше `10 s`, `5 m`, `0.75 m`, приблизно `25%` scale, `20 deg` yaw та novelty `0.12`. Це стартові desktop defaults, не Pi-оптимізована cadence і не flight permission. Descriptor є вхідним evidence; selector не прив'язаний до конкретного VHIX reader або camera pipeline.

Невалідні dimensions, non-finite values, progress поза `0..1`, non-positive scale, unhealthy observation, timestamp/frame rollback і невалідна local-pose quality відхиляються fail-closed.

## Gate Candidate

Verification capture може стати gate-кандидатом лише коли:

- є local pose;
- position uncertainty не перевищує configured maximum;
- `approach_radius - uncertainty` має достатній margin;
- після першого gate є достатня spatial separation від останнього прийнятого gate;
- descriptor novelty відносно останнього прийнятого gate перевищує gate threshold.

Gate novelty навмисно не порівнюється з останнім звичайним verification keyframe: проміжні кадри не повинні змінювати distinctiveness між входами маршруту. Перший якісний local-pose keyframe може започаткувати gate sequence без попередньої separation/novelty reference.

Gate candidate усе ще не є підтвердженим route lock. Майбутній consumer мусить виконати durable native-frame write, package provenance/integrity, camera compatibility, bounded coarse top-N search, high-resolution verification і multi-frame temporal consistency. Лише після цього окремий reviewed reacquisition state machine може створити конкретний `reset_reference`.

## Перевірка

Desktop coverage включає config/input negatives, initial capture, minimum/maximum interval, local і route-relative displacement включно з reverse progress, altitude/scale/wrapped-yaw/novelty triggers, local uncertainty/approach gates, gate separation, gate-to-gate novelty, forged/stale commit, reset invalidation і deterministic trigger reasons.

WSL/GCC та MSVC 19.44/Ninja проходять `42/42`; dedicated selector test додатково проходить `100` повторів на кожній платформі.

## Наступний Slice

1. Додати bounded native-resolution capture/write producer, який викликає `commit()` лише після успішного durable artifact publication.
2. Заповнювати verification chunks і gate records у derived `VHRM`, не перезаписуючи source package.
3. Провести Pi Zero 2W matrix для `1280x800` cadence, RSS, SD latency, temperature, frequency і throttling.
4. Реалізувати bounded offline VHIX scan та top-N provenance output без route lock.
5. Після high-resolution і multi-frame replay acceptance перейти до окремого global reacquisition state machine та точного `reset_reference`.
