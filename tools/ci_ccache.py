"""CI-only compiler launcher; retain real compiler selection and upload guards."""
Import('env')

launcher = env.WhereIs('ccache')
if not launcher:
    raise RuntimeError('CI requested ccache, but ccache is not installed')

# Wrap compilation commands rather than CC/CXX: PlatformIO chooses the target
# toolchain after PRE scripts run. The prefix is inherited by library builds.
# Linking, assembly, compiler flags and embedded source identity stay intact.
env.Replace(
    R1_CCACHE=launcher,
    CCCOM='"$R1_CCACHE" ' + env['CCCOM'],
    CXXCOM='"$R1_CCACHE" ' + env['CXXCOM'],
)
print('R1 CI: ccache enabled for C/C++ compilation')
