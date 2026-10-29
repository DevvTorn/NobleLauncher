# Формат `.Nb` (Noble Build)

`.Nb` — ZIP-контейнер с манифестом `noble.json`. Расширение регистронезависимое.
Формат версионируется полем `formatVersion`; текущая версия — **1**.

## Манифест `noble.json`

```json
{
  "format": "noble-nb",
  "formatVersion": 1,
  "createdBy": "Noble Launcher 2.9.0",
  "createdAt": 1790000000000,
  "embedFiles": true,
  "instance": {
    "name": "Noble Optimized",
    "version": "1.21.1",
    "loader": "NeoForge",
    "loaderVersion": "21.1.80"
  },
  "settings": {
    "ram": 8192,
    "jvmArgs": "-XX:+UseG1GC",
    "gameArgs": "",
    "javaPath": ""
  },
  "content": [
    {
      "kind": "mod",
      "fileName": "sodium.jar",
      "name": "Sodium",
      "version": "0.5.13",
      "author": "CaffeineMC",
      "enabled": true,
      "url": "https://cdn.modrinth.com/...",
      "sha1": "bcdbf37d..."
    }
  ],
  "icon": "icon.png"
}
```

- `kind`: `mod` | `resourcepack` | `shaderpack`.
- `embedFiles: true` — в ZIP лежат сами файлы сборки (кроме `logs`, `crash-reports`, `.cache`, `instance.json`);
  при импорте они распаковываются в новую папку.
- `embedFiles: false` — переносится только манифест и значок; при импорте файлы `content` скачиваются по `url`.
  Элементы без `url` пропускаются с предупреждением.
- `icon` — имя файла значка рядом с манифестом (`icon.png`, `icon.jpg`, `icon.webp`); необязателен.
- `sha1` проверяется при скачивании, если указан.

## Импорт

1. Проверяется ZIP и наличие `noble.json`.
2. `format` должен быть `noble-nb`, `formatVersion` — не выше поддерживаемой.
3. Создаётся новая сборка (папка латиницей, конфликт имён разрешается суффиксом).
4. Распаковываются встроенные файлы либо скачиваются ссылки.
5. Значок копируется в библиотеку иконок сборки.
6. Ошибки показываются в задаче импорта; файлы без ссылок попадают в предупреждения.

## Безопасность

- Пути внутри архива проверяются: запрещены `..`, абсолютные пути и выход за каталог сборки.
- Исполняемые файлы не запускаются; импорт только копирует/скачивает контент.
- Секреты и токены в `.Nb` не включаются.
