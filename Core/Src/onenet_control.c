#include "onenet_control.h"
#include <stdlib.h>
#include <string.h>

/* 使用固定数量的 token，不进行动态内存分配。 */
#define JSMN_STATIC
#define JSMN_PARENT_LINKS
#define JSMN_STRICT
#include "jsmn.h"

#define ONENET_MAX_TOKENS 24U

static uint8_t TokenEquals(const char *json, const jsmntok_t *token,
                           const char *value)
{
    size_t len = strlen(value);
    return (token->end >= token->start &&
            (size_t)(token->end - token->start) == len &&
            strncmp(json + token->start, value, len) == 0) ? 1U : 0U;
}

static uint8_t ReadId(const char *json, const jsmntok_t *token, char *id)
{
    int len = token->end - token->start;
    int i;
    if (token->type != JSMN_STRING || len < 1 || len > 13)
        return 0U;
    for (i = 0; i < len; i++)
    {
        char c = json[token->start + i];
        if (c < '0' || c > '9')
            return 0U;
        id[i] = c;
    }
    id[len] = '\0';
    return 1U;
}

static uint8_t ReadLimit(const char *json, const jsmntok_t *token, float *out)
{
    char value[24];
    char *end;
    double parsed;
    int len = token->end - token->start;
    int i;
    if ((token->type != JSMN_PRIMITIVE && token->type != JSMN_STRING) ||
        len < 1 || len >= (int)sizeof(value))
        return 0U;
    memcpy(value, json + token->start, (size_t)len);
    value[len] = '\0';
    /* Reject nan, inf and hexadecimal spellings before calling strtod. */
    for (i = 0; i < len; i++)
    {
        char c = value[i];
        if (!((c >= '0' && c <= '9') || c == '-' || c == '+' ||
              c == '.' || c == 'e' || c == 'E'))
            return 0U;
    }
    parsed = strtod(value, &end);
    if (end == value || *end != '\0' ||
        !(parsed >= 0.0 && parsed <= 500.0))
        return 0U;
    *out = (float)parsed;
    return 1U;
}

static int SkipSpace(const char *json, int pos, int end)
{
    while (pos < end && (json[pos] == ' ' || json[pos] == '\t' ||
           json[pos] == '\r' || json[pos] == '\n'))
        pos++;
    return pos;
}

/* Check separators explicitly: jsmn is a tokenizer, not a full JSON validator. */
static uint8_t ValidObject(const char *json, const jsmntok_t *tokens,
                           int count, int object)
{
    int i, pos = tokens[object].start + 1;
    int first = 1;
    int end = tokens[object].end - 1;
    for (i = object + 1; i < count; i++)
    {
        int value_start;
        if (tokens[i].parent != object)
            continue;
        if (tokens[i].type != JSMN_STRING || tokens[i].size != 1 ||
            i + 1 >= count || tokens[i + 1].parent != i)
            return 0U;
        pos = SkipSpace(json, pos, end);
        if (!first)
        {
            if (pos >= end || json[pos++] != ',')
                return 0U;
            pos = SkipSpace(json, pos, end);
        }
        first = 0;
        if (pos >= end || json[pos] != '"' || tokens[i].start != pos + 1)
            return 0U;
        pos = SkipSpace(json, tokens[i].end + 1, end);
        if (pos >= end || json[pos++] != ':')
            return 0U;
        pos = SkipSpace(json, pos, end);
        value_start = tokens[i + 1].start -
                      ((tokens[i + 1].type == JSMN_STRING) ? 1 : 0);
        if (pos != value_start)
            return 0U;
        pos = tokens[i + 1].end +
              ((tokens[i + 1].type == JSMN_STRING) ? 1 : 0);
    }
    pos = SkipSpace(json, pos, end);
    return (pos == end && json[end] == '}') ? 1U : 0U;
}

static uint8_t ReadSwitch(const char *json, const jsmntok_t *token, uint8_t *out)
{
    if (token->type != JSMN_PRIMITIVE)
        return 0U;
    if (TokenEquals(json, token, "true") || TokenEquals(json, token, "1"))
        *out = 1U;
    else if (TokenEquals(json, token, "false") || TokenEquals(json, token, "0"))
        *out = 0U;
    else
        return 0U;
    return 1U;
}

uint8_t OneNET_ParseSet(const char *line, OneNET_SetCommand_t *result)
{
    const char *json;
    size_t length;
    jsmn_parser parser;
    /* 仅网络任务调用，固定缓冲区不占用任务栈。 */
    static jsmntok_t tokens[ONENET_MAX_TOKENS];
    OneNET_SetCommand_t command;
    int count, params = -1, id_found = 0, version_found = 0, i;

    if (line == NULL || result == NULL)
        return 0U;
    json = strchr(line, '{');
    if (json == NULL)
        return 0U;
    length = strlen(json);
    while (length > 0U && (json[length - 1U] == ' ' ||
           json[length - 1U] == '\t' || json[length - 1U] == '\r' ||
           json[length - 1U] == '\n'))
        length--;
    memset(&command, 0, sizeof(command));
    jsmn_init(&parser);
    count = jsmn_parse(&parser, json, length, tokens, ONENET_MAX_TOKENS);
    if (count < 1 || tokens[0].type != JSMN_OBJECT ||
        tokens[0].start != 0 || (size_t)tokens[0].end != length ||
        !ValidObject(json, tokens, count, 0))
        return 0U;

    for (i = 1; i < count; i++)
    {
        if (tokens[i].parent != 0)
            continue;
        if (tokens[i].type != JSMN_STRING || tokens[i].size != 1 ||
            i + 1 >= count || tokens[i + 1].parent != i)
            return 0U;
        if (TokenEquals(json, &tokens[i], "id"))
        {
            if (id_found || !ReadId(json, &tokens[i + 1], command.id))
                return 0U;
            id_found = 1;
        }
        else if (TokenEquals(json, &tokens[i], "version"))
        {
            if (version_found || tokens[i + 1].type != JSMN_STRING)
                return 0U;
            version_found = 1;
        }
        else if (TokenEquals(json, &tokens[i], "params"))
        {
            if (params >= 0 || tokens[i + 1].type != JSMN_OBJECT)
                return 0U;
            params = i + 1;
        }
        else
            return 0U;
    }
    if (!id_found || params < 0 || !ValidObject(json, tokens, count, params))
        return 0U;

    for (i = params + 1; i < count; i++)
    {
        if (tokens[i].parent != params)
            continue;
        if (tokens[i].type != JSMN_STRING || tokens[i].size != 1 ||
            i + 1 >= count || tokens[i + 1].parent != i)
            return 0U;
        if (TokenEquals(json, &tokens[i], "pm25_limit"))
        {
            if (command.has_pm25_limit ||
                !ReadLimit(json, &tokens[i + 1], &command.pm25_limit))
                return 0U;
            command.has_pm25_limit = 1U;
        }
        else if (TokenEquals(json, &tokens[i], "buzzer_enable"))
        {
            if (command.has_buzzer_enable ||
                !ReadSwitch(json, &tokens[i + 1], &command.buzzer_enable))
                return 0U;
            command.has_buzzer_enable = 1U;
        }
        else
            return 0U;
    }
    if (!command.has_pm25_limit && !command.has_buzzer_enable)
        return 0U;
    *result = command;
    return 1U;
}
