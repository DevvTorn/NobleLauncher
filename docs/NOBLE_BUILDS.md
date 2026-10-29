# «Сборки от Noble»: лёгкий ZIP-манифест

Кураторские сборки публикуются как **лёгкий ZIP** (можно с расширением `.zip`). Внутри — баннер/иконка
и манифест; тяжёлые файлы модов не входят в архив, лаунчер скачивает их по ссылкам.

## Вариант 1 (рекомендуемый): манифест `.Nb`

ZIP содержит `noble.json` формата `noble-nb` с `embedFiles: false` и списком `content`
(см. `docs/NB_FORMAT.md`). Лаунчер распознаёт его автоматически и устанавливает сборку через
импорт `.Nb`: создаёт инстанс, скачивает моды/ресурспаки/шейдеры по `url`, проверяет `sha1`,
показывает прогресс и предупреждения.

Пример: `assets/noble-builds/noble-sample.zip` (Sodium, Iris, Lithium для Fabric 1.20.1).
Встроенный fallback-манифест ссылается на него как `qrc:/assets/noble-builds/noble-sample.zip`.

## Вариант 2: старый формат

`archiveUrl` может указывать на `.mrpack` или ZIP-экспорт сборки Noble Launcher (`.zip` с `instance.json`).
Такие архивы устанавливаются прежним способом.

## Каталог сборок

`builds/manifest.json` (или `assets/noble-builds-manifest.json` как fallback):

```json
{
  "updatedAt": "2026-10-28",
  "builds": [
    {
      "id": "noble-sample",
      "name": "Noble Sample (Fabric)",
      "author": "Noble Launcher",
      "description": "Лёгкая сборка со ссылками на моды",
      "mcVersion": "1.20.1",
      "loader": "Fabric",
      "loaderVersion": "",
      "banner": "qrc:/assets/logo.png",
      "icon": "qrc:/assets/logo.png",
      "archiveUrl": "qrc:/assets/noble-builds/noble-sample.zip",
      "mods": ["Sodium", "Iris Shaders", "Lithium"],
      "beta": true
    }
  ]
}
```

Лаунчер кэширует удалённый манифест на час; при отсутствии сети использует последнюю копию,
затем встроенный fallback. `archiveUrl` поддерживает `https://`, `file:` и `qrc:/`.
