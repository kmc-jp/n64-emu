namespace N64 {
namespace Cpu {

void Cop1::reset() {
    fcr0.raw = 0;
    fcr31 = 0;
    fgr.fill(0);
}

}