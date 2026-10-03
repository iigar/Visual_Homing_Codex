# GitNexus і Graphify: локальне відновлення 2026-10-03

За прямим запитом користувача після checkpoint `d979b61` відновлено локальні
інструменти навігації. Runtime, алгоритми, конфігурація output і наступний
dataset-validator slice не змінювалися. Pi/camera/FC/UART не використовувалися.

## Що змінено

- GitNexus `1.6.9` замінено для цього checkout на локальний `1.6.12` із npm
  lockfile у `tools/code-graph/`. Глобальний пакет не змінено.
- Новий `gitnexus.mjs` запускає точний локальний CLI без shell, зберігає пробіли
  в аргументах і код завершення. Для `analyze` додає `--index-only`, тому
  індексація не переписує підтримувані AGENTS/CLAUDE та skills.
- FTS-збій відтворено і на новій версії: Windows error 126 через відсутні у DLL
  search path бібліотеки OpenSSL. VC runtime та OpenSSL DLL уже були встановлені.
  Runner додає Git for Windows `mingw64/bin` лише до PATH дочірнього процесу.
  `analyze --repair-fts` завершився успішно; реальні BM25-запити працюють.
- `.gitnexusignore` відсікає reference/artifacts/dependencies/generated content.
  GitNexus лишає canonical docs для пошуку. `.graphifyignore` додатково відсікає
  docs/Markdown/text, щоб Graphify залишався code-only і після `update`.
- `AGENTS.md` тепер містить підтримувані правила проєкту: графи допоміжні;
  UNKNOWN/порожні IDs/нуль результатів/CRITICAL потрібно перевіряти за кодом і
  diff. Поломка графа дозволяє явний fallback на source review і тести.
  `CLAUDE.md` посилається на ті самі правила.
- Локальну конфігурацію Codex MCP переключено з `npx -y gitnexus@latest` на
  Node + цей runner. Попередню конфігурацію збережено поряд із нею як
  `config.toml.gitnexus-before-20261003`; жодної конфігурації профілю в commit немає.

## Перевірки та межі

| Перевірка | Результат |
|---|---|
| GitNexus після повної перебудови, до цього звіту | 277 файлів, 5227 вузлів, 13894 edges, 126 flows; раніше 15698/34129/211 |
| FTS та аргумент із пробілами | `query "refresh route frame health"` успішний; exact-symbol query також працює |
| CLI ↔ окремий MCP stdio-клієнт | `query`, `context`, `impact` мають однакові payloads після виключення лише виміряних timing полів |
| Контроль п'яти документів `6e5964d` → `d979b61` | 20 section symbols, 0 affected processes, risk low; попередні сторонні JSX flows не повторилися |
| C++ контроль `refresh_route_frame_health` | GitNexus знаходить два production caller paths; Graphify — правильні `match_live_camera_route` і `match_replay_route` |
| Python контроль `stop_bindings` | GitNexus знаходить `main` у `scripts/benchmark-recorded-routes.py`, без колишнього Android caller |
| Graphify 0.9.25 після чистого code-only extraction | 209 code files; 3915 вузлів, 6915 edges; 0 duplicate IDs, 0 dangling edges, 0 excluded-path nodes |
| Graphify incremental update після уточнення ignore | No code-graph topology changes; виключені docs/reference не повернулися |
| Runtime source hashes | 217/217 збігаються з перевіреним `6e5964d`; full Debug/Release suite для tooling-only змін не перезапускалася |
| Runner | Node syntax check, version 1.6.12, multiword query та передача помилкового exit code перевірені |

Справність нового MCP-сервера підтверджена окремим клієнтом. **Transport поточної
host-сесії досі closed** навіть після зміни config; його потрібно перепідключити
або почати нову сесію, що читає оновлену конфігурацію. Поточну інтеграцію host
не оголошено відновленою. До цього моменту доступний перевірений локальний CLI.

Графи не стали повними: GitNexus позначає C++ callers як локальні `matcher` і
`health` замість зовнішніх функцій та не показує відомі тестові виклики цього
helper. Analyzer повідомляє 90 відкинутих entry-point candidates і 250
пропущених callee branches. `epistemic=exact` у відповіді не спростовує цих
пропусків. Embeddings не будувалися. Graphify зберігає 614 AST reference/type
вузлів без source file, а `ready.json` не дає AST nodes. Це не доказ повної
call coverage або відсутності залежностей.

## Відтворення й наступна робота

[Команди та MCP-конфігурація](../tools/code-graph/README.md).
Локальні raw logs, standalone MCP probe, CLI/MCP payloads, перевірка hashes і
графових invariants — `artifacts/tooling-repair-20261003/`. Старі indexes та
інструкції збережені в `before/`; runtime/source не відновлювали з backup.

Після цього tooling checkpoint наступний bounded software крок залишається
[offline validator контракту незалежного проходу](SECOND_PASS_DATASET_CONTRACT_UA.md).
Цей ремонт не є новим дозволом на hardware/output або threshold tuning.
