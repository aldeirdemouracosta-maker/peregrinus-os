# Peregrinus OS — Revisão geral após Muro 1.0 / Purgatório 0.1

Data: 2026-10-02.

## Resultado

A base segue coerente e toda a regressão host-side passa com clang/ld.lld, `-Werror`, selo SHA-256 do `.text`, testes de recuperação, TPM, firewall, parser de rede e testes adversariais. O runtime QEMU/OVMF/swtpm e hardware real ainda não foi executado neste ambiente.

## Melhorias aplicadas nesta rodada

1. **LKG aproximada:** Purgatório 0.1 usa como LKG o Muro 1.0.1 gen22 real, mesma security epoch 3. A manutenção Muro 1.0.1 já havia avançado a LKG para o Muro 1.0 gen21 real.
2. **Scripts sem defaults históricos:** `make-boot-request.py` deriva generation/epoch do release profile; `generate-epoch-configs.py` exige metadados explícitos.
3. **Perfis live mutuamente exclusivos:** e1000 qualification não pode ser combinado com recovery/storage-write qualification no mesmo binário; combinação inválida falha em compile-time.
4. **e1000 qualification protegido contra bare metal:** além do macro de build, o driver exige hypervisor CPUID compatível com TCG/QEMU ou KVM antes de habilitar BAR/bus master/DMA.
5. **Menos compilação desnecessária:** e1000, ARP, ICMP, stateful guard, burst guard, datapath e sandbox service saíram do conjunto-base e só são compilados no alvo e1000-live.
6. **Purgatório mínimo:** registro fixo de 32 componentes em quarentena, sem heap, com ID + digest SHA-256, motivos explícitos e saturação fail-closed. Não existe runtime unquarantine nesta fase.

## Excessos removidos/evitados

- Nenhum antivírus, scanner de assinaturas ou filesystem quarantine foi adicionado antes de existir um filesystem/process loader confiável.
- Nenhum NAT, DHCP, IPv6, VLAN, socket API ou TCP stack completo foi adicionado.
- Nenhuma combinação de NIC-live e disk-write-live é permitida no mesmo artefato de qualificação.
- Os drivers live não são mais compilados desnecessariamente no SAFE.
- Não foi criado novo formato persistente para Purgatório; isso evitaria duplicar Recovery Journal/TPM antes de existir necessidade real.

## Pendências reais

1. Executar boot/runtime com QEMU + OVMF + e1000 e, separadamente, OVMF + swtpm.
2. Validar recovery-live com imagem descartável antes de qualquer teste em disco físico.
3. Testar iTCO em X79/Patsburg real antes de liberar `itco_watchdog_arm_live`.
4. Quando houver loader de componentes/filesystem, conectar a API de Purgatório ao ponto de admissão real e definir persistência autenticada se necessário.
5. Criar CI/reprodutibilidade fora deste ambiente, incluindo hash do toolchain.

## Itens conscientemente adiados

- antivírus/malware engine;
- quarentena física de arquivos;
- atualização de vacinas/assinaturas;
- VPN;
- firewall stateful completo/conntrack genérico;
- driver de NIC para hardware físico;
- watchdog físico armado;
- escrita genérica em IA_SYSTEM/IA_DATA;
- reparo GPT automático.

## Veredito técnico

Não há razão para reiniciar a arquitetura. O projeto está mais saudável quando mantém três perfis separados: SAFE, RECOVERY-LIVE/qualification e e1000-QEMU/qualification. A prioridade seguinte deve ser runtime qualification, não adicionar mais subsistemas grandes.

## Atualização posterior

As pendências 1 (parcial) e 5 foram atendidas: boot/runtime QEMU + OVMF dos perfis safe, disk, e1000, journal e commit roda no CI (`docs/QEMU-BOOT-QUALIFICATION.md`), com toolchain LLVM 18 registrada. Continuam pendentes: OVMF + swtpm (cadeia confiável completa), recovery-live em disco físico, iTCO em X79 real.
