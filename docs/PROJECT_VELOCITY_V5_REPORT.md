# Velocity.5 — diagnóstico D3D9, memória temporária e PT-BR

## Estado e escopo

Base: Velocity.4, commit `a20056fa817f614cc77c05d06008fcd28a96ac4e`.
Versão experimental `0.14.0-velocity.5`, código Android 19, ARM64/API 29+.
O relato do usuário é que .4 não melhorou perceptivelmente o desempenho da .3.
Isso impede tratar a alteração dos spinlocks como solução do FPS baixo.
Não recebemos CSV do aparelho correspondente; não há dispositivo Android neste
ambiente. A causa dominante do FPS baixo **continua não confirmada**.

Esta entrega contém mudanças reais e um APK para coleta/testes; **não conclui a
recuperação de FPS nem a integração DXVK 1.10.3**. Não promete 30/60 FPS.
O runtime recompilado, fontes privados, importação do jogo, controles, orientação,
workers e pipeline de downloads verificados foram preservados.

## DXVK 1.10.3: tentativa e bloqueio reproduzível

Backend efetivamente compilado: DXVK Native native-1.9.2b,
`c8dc91fabd00cac11d697ccf07426e798393cd40`, com os patches de
`ports/dxvk_android/prepare.py` e `prepare_velocity4.py`.
Esses patches abrangem intrínsecas ARM, WSI Android/ANativeWindow, escolha do loader
Vulkan, recursos opcionais/BC, resets de command pools, apresentação suboptimal,
HUD, cache e workers. A ponte guest usa interfaces C++ D3D9 nativas; DXVK é ligado
estaticamente dentro de `libnfsu2_android.so`, sem ABI de módulo substituível.

Fonte oficial obtida: doitsujin/dxvk, tag `v1.10.3`, commit
`57740c6ac9c62f3f26c4b5237d7edf3c40797941`.
A primeira compilação ARM64 de `src/d3d9/d3d9_swapchain.cpp` falhou em intrínsecas
x86. Após aplicar os três headers de portabilidade ARM existentes, falhou em:

- `src/util/util_monitor.h`: tipo Win32 `DEVMODEW` ausente;
- `include/vulkan/vulkan_win32.h`: `HINSTANCE` e superfície Win32;
- `src/d3d9/d3d9_cursor.h`: `HCURSOR`;
- `src/d3d9/d3d9_swapchain.cpp`: `WNDPROC`, `LRESULT` e tratamento de mensagens.

Comando reproduzível (retorna o status real de falha do compilador):

```bash
python3 ports/dxvk_android/probe_1103.py /caminho/dxvk-oficial \
  build/dxvk-source "$ANDROID_HOME/ndk/28.2.13676358" build/dxvk-1103-audit
```

O checkout oficial precisa conter a tag: `git fetch origin tag v1.10.3 --depth=1`.
O script valida o commit, extrai a fonte pública, aplica apenas portabilidade de
intrínsecas e grava `android-probe.log`. Nenhum stub Win32 é criado. Isso é uma
sondagem de compilação, não teste funcional nem APK 1.10.3.

A correção restante é portar WSI/monitores/cursor/threading e interfaces native
para 1.10.3, adaptar os patches de capabilities/cache/workers ao novo código,
compilar/linkar um backend completo e testar criação de dispositivo/renderização.
Os erros não significam impossibilidade de portar, mas a integração **não está
implementada/validada nesta release**. Não foi fabricada uma seleção 1.10.3.
O gerenciador mostra a indisponibilidade e mantém downloads de APKs completos.

A informação “última inicialização confirmada” vem de um registro escrito pelo
código nativo depois que Direct3DCreate9 retorna um objeto. Não prova criação de
dispositivo, gameplay ou que o processo ainda esteja executando. O registro é
removido antes do próximo boot e contém a identificação da build/backend.

## Auditoria do caminho de execução

Arquivos inspecionados: guest_runtime.cpp, guest_heap.h, guest_memory.h,
d3d9_bridge.cpp, d3d9_device_methods.inc, d3d9_resource_methods.inc,
d3d9_locks.inc, dxt_shadow.h, velocity_profile.*, velocity_pipeline_cache.h,
DXVK device/swapchain/graphics/compute/pipemanager/state_cache, Activity/JNI,
GameOptions, UiText, DxvkManager e recursos Android.

- Estados D3D9 são repassados ao cache/dirty-state do DXVK. Não se adicionou um
  cache paralelo que poderia ignorar state blocks ou invalidar semântica.
- Draws, render targets, readbacks, locks/unlocks e queries têm identificação por
  método no diagnóstico novo. Não foram removidas fences/barreiras/esperas.
- O código anterior alocava um bloco guest temporário a cada lock e o liberava no
  unlock. O allocator usa `GuestHeaps::alloc(...,8,...)`: procura espaço, atualiza
  mapas/commit e zera o bloco. Depois, locks não DISCARD copiam dados para ele.
  Isso é trabalho repetido confirmado no código, não atribuição comprovada do FPS.
- O runtime estático/HLE continua separado do backend gráfico. CPU process soma
  threads; tempo D3D9 host inclui possíveis esperas e não mede execução da GPU.
- Workers movem trabalho para threads, mas um pipeline obrigatório ainda precisa
  estar pronto antes do draw. .4 sem melhora é compatível com gargalo fora da
  compilação, compilação fria inevitável ou espera de driver, sem provar qualquer
  uma dessas hipóteses.

## Mudanças implementadas

1. **Reutilização de memória de locks:** `acquireLockScratch`, `recycleLockScratch`
   e `clearLockScratch` em d3d9_locks.inc mantêm até 32 blocos de tamanho exato,
   no máximo 8 MiB livres em cache. Locks simultâneos nunca compartilham bloco.
   O conteúdo útil não DISCARD continua sendo copiado; READONLY não copia de volta;
   DXT mantém conversão por região e pitch. Nenhum ponteiro GPU é retido após unlock.
   Padding/DISCARD não têm conteúdo inicial garantido pela API. O cache é liberado
   no shutdown; falha de alocação libera os blocos ociosos antes de uma nova tentativa.
   Limites de offset/tamanho de buffer são conferidos antes do lock nativo.
2. **Métricas D3D9 por método:** contagem exata por par kind/method; tempo das
   primeiras 16 chamadas e depois uma em 64 por método, em vez de depender apenas
   de amostragem global. CSV inclui amostras, soma e máximo observado da sessão.
   São tempos host amostrados, não totais exatos nem medidas GPU. Máximos podem
   omitir outliers não amostrados. Contagens de scratch e bytes de leitura são
   adicionadas por intervalo; a contagem de cópias de upload existente permanece.
3. **Pipelines lentos:** até 32 eventos de readiness >2 ms identificam tipo e hash
   do grupo de shaders, incluindo espera de lock/compilação. O hash não distingue
   sozinho todas as variantes de estado. Registro apenas com profiling ligado,
   limitado também pelo orçamento de metadados de 8 KiB; sem logs por draw.
4. **Cache:** o nome do cache binário inclui `native192b-v5`, além de vendor/device,
   driver/API/UUID. Checksums e validação do header permanecem. O cache binário da
   .4 não é reutilizado nessa namespace: a primeira sessão pode ser mais fria.
   Cache de estados DXVK continua com sua versão interna; algoritmo/ABI inalterados.
5. **Localização:** opções de perfil, shaders, workers, HUD, orientação, cache,
   recuperação, download/instalação e validações agora usam recursos EN/PT-BR.
   Eliminado o construtor de opções com labels literais. Corrigido “Cerrar” no
   menu de controles. Idioma Automático / Português (Brasil) / English persiste
   na preferência existente; não altera arquivos ou idiomas internos do jogo.
   Detalhes técnicos externos (driver, sistema, servidor) podem manter seu texto
   original, acompanhados por mensagem localizada; nomes de GPU/ABI não se traduzem.
6. **Ferramenta de comparação:** `scripts/compare_velocity_reports.py` aceita CSVs
   .4/.5, exige descrição do cenário e calcula FPS médio, 1% low e percentis,
   além de ordenar operações D3D9 pelo custo estimado. Métricas ausentes não são
   inventadas. O CSV contém no máximo os 600 intervalos mais recentes; operações
   D3D9 são totais amostrados da sessão, não da mesma janela.

## Evidência e testes

Teste controlado de componente: 1.000 locks sequenciais de 65.536 bytes:
**1 alocação e 999 reutilizações**. O caminho anterior fazia uma alocação por lock.
É uma contagem verificada no host, não benchmark Android nem ganho de FPS.
O teste cobre blocos ativos distintos, limite de cache, descarte de blocos grandes
 e limpeza; testes existentes verificam DXT1/3/5, pitch, regiões, READONLY e guards.

- Dez testes CTest (incluindo memória, textura, profiler ligado/desligado,
  pipelines/cache, concorrência/timeout e pacing): aprovados.
- Testes graphics_memory_check e graphics_com_check: aprovados.
- Quatro testes de recursos/localização/fallback e três de rotação: aprovados.
- Dois testes do parser comparativo: aprovados (fixtures sintéticas, sem FPS real).
- DXVK 1.10.3: compilação de sondagem falhou como descrito; renderização não testada.
- ARM64 de componentes, APK, JNI, assinatura e CI: resultados finais registrados
  com o artefato e nas notas da pré-release.

## Matriz de hardware e comparação

| Backend/build | Adreno | Mali/Immortalis | Xclipse | PowerVR/outras |
| --- | --- | --- | --- | --- |
| .4 native-1.9.2b | usuário relata sem melhora; sem CSV nesta sessão | não medido aqui | não medido aqui | não medido aqui |
| .5 native-1.9.2b | validação física pendente | validação física pendente | validação física pendente | validação física pendente |
| .5 DXVK 1.10.3 | indisponível | indisponível | indisponível | indisponível |

Nenhum fabricante recebe exclusão ou extensão forçada. Os perfis existentes têm
orçamento real de workers/backbuffers; detecção Vulkan e fallbacks permanecem.
Vulkan anunciado sozinho não garante os recursos necessários ao port.
Não foi testada instalação física, corrida, áudio, save, integridade visual ou
mudança de driver nesta máquina. Não há comparação FPS antes/depois medida aqui.

## Reproduzir e investigar

1. Instalar .5 sobre .4 (mesmo certificado/pacote), importar a pasta como antes.
2. Selecionar idioma; ativar profiling e manter o mesmo HUD, resolução, driver,
   limite de FPS, perfil, modo de shaders e save/cena usados na comparação.
3. Separar ensaios de cache frio/quente, aquecer o aparelho de maneira equivalente,
   registrar temperatura/configuração e repetir a mesma corrida por igual duração.
4. Pausar para exportar o CSV; salvar relatórios com GPU/driver/build/configuração.
5. Comparar: `python3 scripts/compare_velocity_reports.py --scenario
   'mesmo save/cena/resolução/driver e cache quente' v4.csv v5.csv > comparison.json`.
   Não fornecer um terceiro arquivo “1.10.3” até existir backend realmente funcional.
6. Para kind 2, métodos 81–84 são draws, 32/33 readbacks, 57 render state,
   65 textura, 69 sampler, 94/109 constantes de shaders. Kinds 6/7: métodos 11/12
   locks/unlocks de VB/IB; kinds 3/4: 19/20 locks/unlocks de texturas/cubos.
   Custos amostrados ajudam escolher a próxima medição, não confirmam causalidade.
7. Comparar também profiling/HUD desligados para avaliar overhead. GPU timestamp
   real continua indisponível; use Perfetto/AGI quando o aparelho permitir.

## Regressões possíveis e reversão

O cache de scratch aumenta a memória retida em até 8 MiB e pode reduzir memória
livre disponível ao guest. O fallback em falha de alocação libera o cache ocioso.
Se surgir corrupção de locks, reverter a alteração de d3d9_locks.inc e reconstruir;
reprodução visual em hardware ainda é obrigatória. Não reduzir resolução ou
ignorar sincronização para esconder falhas.

É possível desativar profiling e usar shaders síncronos na UI. A .4 permanece
publicada, mas Android não aceita downgrade normal sobre código 19. Não instruir
limpeza de dados sem backup de saves; recompilar o código anterior com versionCode
maior é alternativa de recuperação assinada. Não foi implementado rollback de APK.

## Artefato final

- `NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.5.apk` — 62194032 bytes.
- SHA-256: `9fd948f206cb4718396348315d66d43b1033451485e9ec56e2f2b45e5f509db7`.
- Assinatura APK v2 de teste, certificado SHA-256 `a9f5483dbd5a28e74164d47cfbfa224a5532075a40ae3f90ec56602023790b18`, igual à .4.
- Seis bibliotecas ARM64, 20 funções JNI e NativeActivity verificados estaticamente; sem imports fortes ausentes contra libs empacotadas e stubs NDK API 29.
- ZIP íntegro, alinhamento de APK/ELF 16 KiB, manifesto código 19/minSDK 29, recursos PT-BR presentes; não debuggable.
- Compilação ARM64 dos dez componentes aprovada; 54 módulos SPIR-V aprovados para Vulkan 1.1.
- Política Java de HTTPS, SHA-256, identidade ELF e dependências aprovada contra o APK final.
- Instalação e gameplay em aparelho não executados.
