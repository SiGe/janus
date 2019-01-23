def Settings( **kwargs ):
  return {
    'flags': ['-O3', '-Wall', '-Werror', '-Iinclude/', '-std=gnu11', '-fms-extensions', '-Wno-microsoft-anon-tag', '-Ilib/', '-pthread']
  }

