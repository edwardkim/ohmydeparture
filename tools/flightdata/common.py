"""Shared configuration, atomic storage, redaction and request accounting."""
import json
import os
from pathlib import Path
import re
import shlex
import tempfile
from urllib.parse import quote, unquote, urlencode
from urllib.request import HTTPRedirectHandler


class DataError(ValueError):
    pass


def read_env(path, names):
    values = {}
    for number, line in enumerate(Path(path).read_text(encoding='utf-8-sig').splitlines(), 1):
        match = re.match(r'^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$', line)
        if not match or match[1] not in names:
            continue
        try:
            parts = shlex.split(match[2], comments=True)
        except ValueError:
            raise DataError(f'Invalid configuration quoting on line {number}') from None
        if len(parts) != 1 or not parts[0]:
            raise DataError(f'{match[1]} must have one nonempty value')
        values[match[1]] = parts[0]
    return values


def atomic_write(output, text, mode=0o600, prefix='.flight-data-'):
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=output.parent,
                                         prefix=prefix, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(text)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.chmod(mode)
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def redact(text, key):
    if not key:
        return text
    variants = {key, unquote(key), quote(key, safe=''),
                urlencode({'serviceKey': unquote(key)}).split('=', 1)[1]}
    for value in sorted(variants, key=len, reverse=True):
        if value:
            text = text.replace(value, '[REDACTED]')
    return re.sub(r'(?i)((?:access_key|serviceKey)=)[^&\s"<>]+', r'\1[REDACTED]', text)


def reject_credentials(value):
    secret_names = {'access_key', 'servicekey', 'icn_key', 'aviationstack_key', 'acode_key', 'authorization'}
    if isinstance(value, dict):
        for key, item in value.items():
            if key.lower() in secret_names and item not in (None, '', '[REDACTED]'):
                raise DataError('Snapshot contains credentials; remove them before activation')
            reject_credentials(item)
    elif isinstance(value, list):
        for item in value:
            reject_credentials(item)
    elif isinstance(value, str):
        for match in re.finditer(r'(?i)[?&](?:access_key|serviceKey)=([^&\s"<>]+)', value):
            if match[1] != '[REDACTED]':
                raise DataError('Snapshot contains a credential-bearing URL')


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


class RequestBudget:
    def __init__(self, maximum):
        if maximum < 0:
            raise DataError('Request budget must be nonnegative')
        self.maximum, self.used = maximum, 0

    def call(self, function, *args, **kwargs):
        if self.used >= self.maximum:
            raise DataError('Request budget exhausted; no further request was sent')
        self.used += 1
        return function(*args, **kwargs)


def data_home():
    base = Path(os.environ.get('XDG_DATA_HOME', ''))
    return base if base.is_absolute() else Path.home() / '.local/share'


def state_home():
    base = Path(os.environ.get('XDG_STATE_HOME', ''))
    return base if base.is_absolute() else Path.home() / '.local/state'


def json_text(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + '\n'
