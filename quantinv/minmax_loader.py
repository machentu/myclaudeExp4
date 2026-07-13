"""
Parse the floatminmax_txt file to extract per-layer min/max quantization parameters.

Expected file format (one of):
  1. "layer_name min_value max_value" per line (space/tab separated)
  2. "layer_name: min=max" per line
  3. JSON format: {"layer_name": {"min": val, "max": val}, ...}

If the file is empty or missing entries for a layer, default placeholder values are used.
"""

import os
import json
from errors import ReadError, MeanError, VarError


def load_minmax(path_float_minmaxtxt):
    """Parse the min/max text file and return per-layer quantization data.

    Supports multiple file formats:
    1. Reference tool format: multi-line blocks with "the name/maxvalue/minvalue of this layer"
    2. Simple format: "layer_name min_value max_value" per line
    3. Colon format: "layer_name: min_value max_value" per line
    4. JSON format: {"layer_name": {"min": val, "max": val}, ...}

    Returns:
        dict: {layer_name: {"min": float, "max": float}}

    Raises:
        ReadError: if file not found or unreadable
        MeanError/VarError: if values are malformed
    """
    if not os.path.isfile(path_float_minmaxtxt):
        raise ReadError("Min/max file not found: %s" % path_float_minmaxtxt)

    try:
        with open(path_float_minmaxtxt, 'r') as f:
            content = f.read().strip()
    except Exception as e:
        raise ReadError("Failed to read min/max file: %s" % str(e))

    if not content:
        return {}

    result = {}

    # Try JSON format first
    try:
        data = json.loads(content)
        if isinstance(data, dict):
            for k, v in data.items():
                _validate_and_store(k, v, result)
            return result
    except (json.JSONDecodeError, TypeError):
        pass

    # Try reference tool multi-line format
    ref_result = _parse_reference_format(content)
    if ref_result:
        return ref_result

    # Try line-based formats
    for line_num, line in enumerate(content.split('\n'), 1):
        line = line.strip()
        if not line or line.startswith('#'):
            continue

        entry = _parse_line(line, line_num)
        if entry is not None:
            name, min_val, max_val = entry
            _validate_and_store(name, {"min": min_val, "max": max_val}, result)

    return result


def _parse_reference_format(content):
    """Parse the reference tool's multi-line minmax format.

    Format for each layer:
        the index of layer is X
        the name of layer in floatpb is:layer_name:0
        the maxvalue of this layer is:0.1
        the minvalue of this layer is:0.0

    Returns dict or None if format not matched.
    """
    lines = content.split('\n')
    result = {}
    i = 0

    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue

        # Look for "the name of layer in floatpb is:"
        if 'the name of layer in floatpb is:' in line.lower():
            # Extract layer name (format: "is:layer_name:0")
            try:
                name_part = line.split('is:')[1].strip()
                # Remove ':0' suffix if present
                name = name_part.replace(':0', '').strip()

                # Next lines should have maxvalue and minvalue
                max_val = None
                min_val = None
                j = i + 1
                while j < len(lines) and j < i + 5:
                    next_line = lines[j].strip()
                    if 'the maxvalue' in next_line.lower() and 'is:' in next_line:
                        max_part = next_line.split('is:')[1].strip()
                        max_val = float(max_part)
                    elif 'the minvalue' in next_line.lower() and 'is:' in next_line:
                        min_part = next_line.split('is:')[1].strip()
                        min_val = float(min_part)
                    elif next_line and not next_line.startswith('the '):
                        break
                    j += 1

                if name and max_val is not None and min_val is not None:
                    result[name] = {"min": min_val, "max": max_val}

                i = j
            except (ValueError, IndexError):
                i += 1
            continue

        i += 1

    return result if result else None


def _parse_line(line, line_num):
    """Parse a single line of the minmax file. Returns (name, min, max) or None."""

    # Skip reference tool meta lines like "the index of layer is 0"
    if line.startswith('the ') or line.startswith('The '):
        return None

    # Format: "the name of layer in floatpb is:layer_name:0"
    if 'the name of layer' in line.lower():
        return None

    # Format: "the maxvalue of this layer is:0.1"
    if 'the maxvalue' in line.lower() or 'the minvalue' in line.lower():
        return None

    # Format: "layer_name: min_value max_value"
    if ':' in line:
        parts = line.split(':', 1)
        name = parts[0].strip()
        rest = parts[1].strip()

        # Try "min=max" format
        if '=' in rest and 'min' in rest.lower():
            tokens = rest.replace(',', ' ').split()
            vals = [t for t in tokens if t not in ('min', 'max')]
            if len(vals) >= 2:
                return name, float(vals[0]), float(vals[1])

        # Try "min_value max_value" after colon
        tokens = rest.split()
        if len(tokens) >= 2:
            try:
                return name, float(tokens[0]), float(tokens[1])
            except ValueError:
                raise VarError("Invalid min/max on line %d: %s" % (line_num, line))

        return None

    # Format: "layer_name min_value max_value" (space/tab separated)
    tokens = line.split()
    if len(tokens) >= 3:
        try:
            return tokens[0], float(tokens[1]), float(tokens[2])
        except ValueError:
            raise VarError("Invalid min/max on line %d: %s" % (line_num, line))

    return None


def _validate_and_store(name, value, result):
    """Validate and store a min/max entry."""
    if isinstance(value, dict):
        min_val = value.get('min')
        max_val = value.get('max')
    elif isinstance(value, (list, tuple)) and len(value) >= 2:
        min_val, max_val = value[0], value[1]
    else:
        raise MeanError("Invalid min/max entry for layer '%s': %s" % (name, value))

    if min_val is None or max_val is None:
        raise VarError("Missing min or max for layer '%s'" % name)

    try:
        min_val = float(min_val)
        max_val = float(max_val)
    except (TypeError, ValueError):
        raise VarError("Non-numeric min/max for layer '%s': %s, %s" % (name, min_val, max_val))

    if min_val >= max_val:
        raise VarError("min >= max for layer '%s': %s >= %s" % (name, min_val, max_val))

    result[name] = {"min": min_val, "max": max_val}
