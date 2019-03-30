def Settings( **kwargs ):
  return {
    'flags': [
        '-O3', '-std=gnu11', '-fms-extensions',
        '-Wno-microsoft-anon-tag', 
        '-Iinclude/', '-Ilib/', '-pthread',
        '-Wall', '-Werror', '-pedantic']
  }

