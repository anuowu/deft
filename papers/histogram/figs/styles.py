_colors = { 'nw': 'r',
            'kT0.4': 'c',
            'kT0.5': 'm',
            'kT1': 'g',
            'tmmc': 'k',
            'oetmmc': 'b',
            'wang_landau': 'g',
            'vanilla_wang_landau': 'b',
            'simple_flat': 'r',
            'optimized_ensemble': 'y'}

def color(method):
    if method in _colors:
        return _colors[method]
    return 'b' # everything else blue

def line(method):
    lines = { 'nw': '--',
              'tmmc': ':'}
    if method[:2] == 'kT':
        return '--'
    if method in lines:
        return lines[method]
    return '-'

def title(method):
    titles = { 'nw': '$kT/\epsilon = \infty$ sim.',
               'tmmc': 'tmmc',
               'oetmmc': 'oetmmc',
               'wang_landau': 'Wang-Landau',
               'vanilla_wang_landau': 'Vanilla Wang-Landau',
               'simple_flat': 'simple flat',
               'optimized_ensemble': 'optimized ensemble'}
    if method in titles:
        return titles[method]
    if method[:2] == 'kT':
        return '$kT/\epsilon = %g$ sim.' % float(method[2:])
    return 'unrecognized method'

def plot(method):
    if method in _colors:
        return color(method) + line(method)
    return line(method)

def dots(method):
    if method in ['wang_landau','vanilla_wang_landau','simple_flat']:
        return color(method) + '+'
    if method in _colors:
        return color(method) + '.'
    return '.'
