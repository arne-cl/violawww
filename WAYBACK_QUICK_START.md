# Web Archive Integration - Быстрый старт

## Что это делает

Когда домен не существует (DNS ошибка), ViolaWWW **автоматически** открывает **самую раннюю** версию сайта из Internet Archive.

## Использование

### Просто запустите браузер:
```bash
./src/vw/vw http://www.hackzone.ru/
```

Браузер автоматически:
1. Обнаружит, что DNS не работает
2. Проверит Web Archive
3. Найдёт снимок от **1997-03-27** (самый ранний!)
4. Откроет: `https://web.archive.org/web/19970327140651/http://www.hackzone.ru:80/`

## Тестирование

```bash
make test
```

Запустит все тесты, включая Web Archive интеграцию.

## Технические детали

- **API**: [CDX Search](https://web.archive.org/cdx/search/cdx) от Internet Archive
- **Формат**: Простой текстовый (space-separated)
- **Сортировка**: `sort=ascending` → самый ранний снимок
- **Таймауты**: 5 сек (Web Archive), 30 сек (HTTP/HTTPS)
- **Логи**: Только если `TRACE` включен

## Полная документация

См. `WAYBACK_FEATURE.md` для детальной информации.

