# Project Velocity ARM64 — 0.14.0-velocity.4

Versão experimental baseada na Velocity.3: tag `v0.14.0-velocity.3`, código
`9131fb0`, com correção de teste `4917f31`. DXVK Native permanece fixado em
`c8dc91fabd00cac11d697ccf07426e798393cd40` (native-1.9.2b), com patches Android.
Não se trata de uma release oficial do upstream. Package preservado:
`com.nfsu2.androidevolved`, versionCode 18, ARM64, Android 10/API 29+.

## Diagnóstico e comparação com .3

O usuário relatou na Adreno 610/PurpleVK/Vulkan 1.3.305: 2,4 FPS instantâneos,
8,3 FPS médios, último intervalo 513,6 ms, CPU do processo 437,61 ms e 24 pipelines.
São dados de teste do usuário. Não recebemos uma captura CSV correspondente;
resolução, cena, estado térmico e distribuição dos custos não foram verificados.
CPU do processo soma várias threads e não é tempo da GPU nem atribuição exata ao
código recompilado. A contagem de 24 pipelines não demonstra compilação contínua.

A auditoria cobriu guest_runtime/GuestMemory, flags e dispatch do C gerado, ponte
D3D9, locks/uploads DXT, perfilador/HUD, DXVK graphics/compute/state cache/pipeline
manager, JNI, Activity, launcher, manifest e empacotamento. A otimização existente
de memória, hash dispatch, registradores locais, x87, ThinLTO, conversão DXT por
região e sincronizações necessárias foi preservada.

**Confirmado no código:** .3 usa spinlocks ao redor da criação de pipelines, uma
operação potencialmente longa no driver. A pré-compilação existente usa até cinco
workers numa CPU de oito núcleos. O cache de estados já permite aquecimento assim
que os shaders necessários se tornam conhecidos. Não havia uma ABI pública para
substituir DXVK: ele é uma biblioteca estática dentro de libnfsu2_android.so.

**Ainda hipotético no aparelho:** quanto do FPS baixo vem de espera ativa,
compilação, runtime x86→ARM64, DXT, apresentação, GPU/driver ou sincronização.
Reduzir a resolução sem melhora não prova sozinho um gargalo específico.

Experimento sintético reproduzível no host x86_64, GCC -O2, usando o spinlock real
do DXVK e uma seção protegida de 50 ms:

| Rodada | CPU da thread esperando com spinlock (ms) | Com mutex bloqueante (ms) |
| --- | ---: | ---: |
| 1 | 50,0736 | 0,035642 |
| 2 | 50,1147 | 0,023865 |
| 3 | 50,1003 | 0,019018 |

Isso verifica o custo da espera ativa nesse cenário; **não é benchmark de Android,
compilação de driver ou FPS**. Fonte: `tests/velocity/pipeline_lock_bench.cpp`.
Compile com `-DDXVK_NATIVE`, os includes do DXVK preparado e `-pthread`.

A comparação arquitetural com Winlator permanece no relatório da .3. Wine/Box64,
DXVK Windows e driver configurável diferem do runtime estático + HLE + DXVK Native.
Não foi confirmado qual container/driver/DXVK do Winlator reproduz o teste do
usuário. PurpleVK é o driver informado para o APK; não foi extraído ou redistribuído.

## Implementação de shaders e pipelines

`velocity_async.h` e `ports/dxvk_android/prepare_velocity4.py` integram uma fila
compartilhada com até quatro workers, usada tanto para tradução DXSO→SPIR-V quanto
para pipelines e pré-compilação. Trabalho necessário ao jogo tem prioridade sobre
o aquecimento de estados conhecidos. A fila é limitada a 1024 tarefas; saturação
faz o trabalho necessário executar sincronamente. Duplicatas de grupos de shaders
no aquecimento são filtradas. Shaders e objetos de pipeline existentes são reutilizados.

Os locks longos de graphics/compute usam mutex bloqueante. Estados compilados por
workers continuam sendo gravados no cache de estados; o cache binário persistente
por GPU/driver/API/UUID da .3 permanece. Handles de pipeline nulos não entram no
cache. Falha de pipeline em worker é propagada e há uma tentativa síncrona; falha
persistente é sinalizada pela ponte D3D9 ao runtime após a apresentação,
sem encerrar silenciosamente a thread CS nem armazenar pipeline fictício. Falhas de aquecimento
são registradas e o primeiro uso pode tentar novamente.

O chamador conserva objetos e dados até o resultado ficar pronto. **Não se pula
nenhum draw para esconder uma compilação.** Um shader/pipeline novo que não foi
pré-compilado ainda exige espera no primeiro uso. Aquecimento ocorre quando o jogo
carrega/registra shaders cujos estados foram salvos, inclusive durante carregamentos;
não há previsão de todos os shaders ainda desconhecidos nem alteração da lógica do jogo.

Após dois segundos, uma tarefa ainda na fila pode ser cancelada atomicamente e
executada sincronamente uma única vez. Uma chamada Vulkan que já começou não pode
ser encerrada com segurança: registra-se a espera longa e aguarda-se o resultado.
Não há promessa de recuperação de um driver permanentemente travado. O encerramento
aguarda futuros antes de liberar objetos, evitando jobs que referenciem objetos destruídos.

Configurações funcionais: Async Auto/On/Off, workers Auto/1/2/3/4, prewarming On/Off,
cache persistente existente e limpeza dos caches na próxima inicialização. Auto
usa critérios conservadores de CPU/RAM; CPUs com menos de quatro núcleos ou menos
de 2 GiB usam fallback síncrono. Até quatro núcleos usam um worker automático;
até oito, dois; acima disso, três. Até 3 GiB limita o orçamento a um worker.
Pedidos explícitos continuam limitados pelo orçamento e pelos núcleos disponíveis.
O mesmo orçamento serve todas as famílias de GPU, sem listas por nome comercial.

Perfis: Compatibility usa um worker automático e um backbuffer; Balanced mantém
opções existentes e orçamento conservador; Performance solicita até três workers
automáticos e dois backbuffers; Experimental solicita até quatro. Nenhum perfil
reduz qualidade automaticamente ou habilita extensões indisponíveis. Uma sessão
foreground encerrada inesperadamente deixa um marcador privado; a próxima
inicialização usa shaders síncronos e perfil conservador para essa execução.
Isso é recuperação de configuração, não rollback de APK nem diagnóstico da causa
do encerramento. Pausa normal remove o marcador.

## Capacidades e métricas

Probe acrescenta tamanho dos heaps locais (não memória livre), timestampPeriod e
bits válidos por queue family, além de anunciar GPL, cache control e pipeline
binary quando o driver os lista. O backend 1.9.2b não implementa esses três caminhos
avançados nesta versão; extensão anunciada não significa recurso habilitado.
O probe separado pode selecionar um adaptador diferente: o HUD e metadados DXVK
identificam o adaptador efetivamente usado pelo jogo.

HUD detalhado e CSV incluem média, 1% low (inverso da média dos intervalos mais
lentos, arredondando o número de amostras para cima), P50/P95/P99 por nearest rank,
com até 600 intervalos. O HUD atualiza estatísticas formatadas duas vezes por
segundo; compacto não ordena percentis. O CSV de .4 tem 54 colunas, incluindo CPU
do processo e CPU da thread que chama a apresentação, draw calls, tempos de host,
espera por pipeline pronto, hits/misses de objetos DXVK, falhas de prewarming e
esperas do compilador acima de dois segundos. A thread muda: o intervalo de CPU
da thread é zero nesse caso, não uma subtração entre clocks de threads diferentes.

O cabeçalho fornece traduções de shaders e compilações de pipelines por segundo,
e intervalos lentos que coincidem com conclusão de compilação. Correlação não é
causalidade. Hits/misses são do cache de objetos DXVK, **não** feedback do cache
opaco do driver. Tempo real de execução da GPU continua indisponível: não se usa
tempo de vkQueuePresent/submit como substituto. Scopes paralelos/aninhados podem
sobrepor-se. Profiling permanece opt-in; exportação e cache ficam fora do frame path.

## DXVK Manager: funcionalidade e limite exato

O gerenciador baixa **APKs completos correspondentes de DXVK + runtime**, não
módulos substituíveis ou DLLs Windows. A integração atual é estática e não oferece
ABI de plugin, ownership e callbacks versionados que permitam trocar só a biblioteca
com segurança. Android não proíbe todo carregamento dinâmico; a limitação aqui é a
arquitetura do port e a ausência de builds modulares validadas. Portanto, o pedido
de troca independente de versões upstream DXVK permanece pendente.

Implementado: catálogo remoto versionado no repositório autorizado, HTTPS com
origem/redirects permitidos, progresso, cancelamento, limites de tamanho/tempo,
SHA-256, verificação ELF64 AArch64, DT_NEEDED contra libs empacotadas/API Android,
manifest/versionCode/package e certificado igual ao app instalado. Download usa
arquivo temporário e rename; só o pacote verificado é oferecido ao instalador.
A leitura ELF usa intervalos pequenos, sem carregar uma biblioteca de 150 MiB na
memória Java. Um provider privado e somente leitura concede acesso ao instalador.
O Android ainda valida a assinatura e pede confirmação; a atualização reinicia o app.

Categorias só listam builds reais publicadas: Legacy .3 e Experimental .4, ambas
com native-1.9.2b e conjuntos distintos de patches Velocity. Current é o backend
empacotado. Auto conserva o instalado porque não há alternativa mais rápida
validada para o dispositivo. Stable não inventa uma build validada em hardware.
Downloads podem ser excluídos; o backend ativo não é apagado.

Instalação cancelada/falha mantém o APK anterior por meio do instalador Android.
A ação Recovery seleciona o backend empacotado com compilação síncrona e perfil
conservador. **Não há downgrade silencioso/rollback automático de APK instalado**:
Android impede downgrade normal. Downloads legados são identificados como tal.
Desinstalar para downgrade pode remover dados; preserve saves antes de fazê-lo.
Troca/instalação e diálogos Android ainda precisam de validação física.

## Build e testes

Toolchain: NDK 28.2.13676358, CMake 3.30.5, ARM64/API 29, Release não debuggable,
compilação guest -O2/ThinLTO/proteções e importação originais. Entrada e fontes
recompilados privados legítimos foram reutilizados; nenhum stub, executável comercial,
fonte derivado ou chave de assinatura foi adicionado ao GitHub.

Validação automatizada:

- Dez testes CTest no host: profiling ligado/desligado, memória, DXT, frame pacing,
  máscara Vulkan 1.1, frame stats/percentis, cache persistente, surface policy e
  compilador assíncrono. Este último testa prioridade, propagação de erros, limite
  da fila, fallback síncrono, timeout antes de executar sem duplicação e shutdown.
- Compilação dos dez componentes para ARM64; não executados em Android.
- Verificações de regressão de memória/proteções, texturas/DXT/COM, PCM e arquivos.
- Verificações de integração da orientação e regressões dos recursos de idiomas.
- Teste Java da política de URLs/redirects/SHA/ABI e dependências, usando um APK
  real como caso válido e rejeitando pacote de ABI incorreta.
- Build completo do APK, shaders SPIR-V Vulkan 1.1, exports JNI, dependências
  nativas, ZIP, assinatura e alinhamento de 16 KiB; resultados finais na release.

Falhas de desenvolvimento corrigidas: Rc do DXVK exige comparação explícita com
nullptr; a nova consulta de memória Vulkan deve usar a função do driver selecionado,
resolvida por vkGetInstanceProcAddr, não um símbolo global de outro loader.

Nenhum dispositivo Android esteve disponível. Não foram executados testes reais
de instalação, launcher, importação, gameplay, gráficos, saves, controles, rotação,
retomada, consumo térmico ou troca de pacote. A compilação não demonstra essas funções.

## Matriz de dispositivos

| Família | Política | Testes reais .4 |
| --- | --- | --- |
| Adreno 500/600/700+ com Vulkan suficiente | Capacidades/formatos do driver, mesmo pool e fallbacks | Não validados |
| Mali G | Mesmos critérios; sem Turnip presumido | Não validados |
| Immortalis | Mesmos critérios | Não validados |
| Xclipse/RDNA | Mesmos critérios; sem extensões desktop presumidas | Não validados |
| PowerVR | Mesmos critérios, fallback DXT existente | Não validados |
| Outras Vulkan ARM64 | Requisitos reais do backend, sem exclusão por fabricante | Não validados |

Para cada família, pendem: inicialização, importação, menu, corrida, gameplay,
integridade visual, shaders/cache, instalação de pacote, recuperação, FPS sustentado
e memória. Não há comparação de FPS .3/.4 medida no mesmo aparelho.

## Reproduzir e reverter

1. Use .3 e .4 no mesmo aparelho, driver, cena/save, resolução, limite FPS e estado
   térmico. Registre CPU/GPU/API/driver e opções; o número Vulkan sozinho não define
   desempenho. Mantenha importação e qualidade iguais.
2. Capture até 600 intervalos com profiling ligado, depois pause e exporte o CSV.
   Compare CPU da thread de apresentação versus CPU total, P95/P99/1% low, draw
   calls, pipeline_ready_wait, compilações, uploads, DXT e waits. Não some scopes
   sobrepostos como se fossem partes exclusivas do frame.
3. Separe cache frio/quente e Auto/On/Off; compare HUD Off/compacto/detalhado.
   Faça pausa normal para persistir o cache. Limpeza ocorre no próximo lançamento,
   evitando apagar arquivos enquanto o renderer os utiliza. Perfetto pode ajudar
   a atribuir o tempo de CPU às threads; GPU timestamps não estão implementados.
4. Teste todos os casos da matriz e rotação normal/invertida/automática, inclusive
   pausa/retomada. Preserve screenshots/capturas de geometria para detectar regressões.
5. Para recuperar, use Recovery no DXVK Manager ou Async Off, prewarming Off e
   Compatibility. .3 permanece na release anterior. Para rollback de fontes,
   reverta commits .4 e recompile com os mesmos inputs privados; não substitua
   funções do jogo por stubs.

## Critérios ainda não satisfeitos integralmente

- Ganho sustentado de FPS e ausência de falhas gráficas/deadlocks no jogo exigem
  teste físico; testes de concorrência cobrem o componente, não todos os drivers.
- Tempo GPU, feedback de cache do driver, GPL/cache-control/pipeline-binary e
  escalonamento dinâmico de resolução por tempo GPU permanecem indisponíveis.
- Gerenciador de módulos DXVK independentes e rollback automático de APK não foram
  implementados. O download verificado de pacotes completos é a alternativa real.
- Uma compilação Vulkan já em execução não pode ser abortada com segurança.

Estas limitações são declaradas na pré-release. Não há promessa de “tudo funcionando”
em GPUs que não foram testadas, nem ganhos de FPS inventados.

## Artefato final verificado

- APK: `NFSU2-Project-Velocity-ARM64-v0.14.0-velocity.4.apk`
- Versão: `0.14.0-velocity.4`, versionCode 18, Android 10+.
- Tamanho: 62210656 bytes.
- SHA-256: `346476c9b074a8a7ee1ac8144fdeb95c92435382c6b44159575b608963e87a34`
- Assinatura APK v2 de desenvolvimento, mesmo certificado da .3: `a9f5483dbd5a28e74164d47cfbfa224a5532075a40ae3f90ec56602023790b18`.
- Seis bibliotecas ARM64, alinhamento 16 KiB, 20 exports JNI e NativeActivity verificados estaticamente; sem imports fortes ausentes contra bibliotecas empacotadas e stubs NDK API 29.
- ZIP, assinatura, política Java de dependências e dez testes CTest aprovados no artefato/código final. Instalação e gameplay físicos pendentes.
