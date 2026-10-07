#!/usr/bin/env python3
"""Reviewed display units: Controller Manual v3.0c and RoboRun+ 3.3 v3 trees.
Only presentation metadata is changed. The one corrected scale is TID (A x10,
manual p449); raw protocol integers, bounds and edit permissions are preserved.
"""
import json
import sys
from pathlib import Path

GROUPS = {
    'mV': 'ACTR AMAX AMIN',
    '%': 'ADB PDB ICAP MXPF MXPR',
    'ms': 'ATGD BKD FET CHB CSRT CTPS',
    'K': 'B25',
    'Hz': 'LPFB',
    'Ω': 'RS R25 TRPU',
    'pulsos/volta': 'EPPR',
    'contagens': 'EHL EHOME ELL SHL SHOME SLL BHL BHOME BLL',
    'bits': 'MCLE SCLE SLEN',
    'bit inicial': 'MSTA SSTA',
    'pares de polos': 'BPOL SPOL',
    '1/512 volta': 'BADJ BADV HSAT PSA',
    'ID': 'MNOD CLSN CNOD',
    'comando': 'DFC',
    'inteiro': 'EE',
    'texto': 'TELS',
    'calibração': 'ZSMA ZSMC',
    'mapa': 'HSM',
    'máscara': 'AINA AMAXA AMINA DINA DOA PINA PMAXA PMINA ATGA EHLA ELLA EMOD SHLA SLLA SMOD BHLA BLLA',
    'opção': 'ACS AMS BRUN CLIN CPRI DMOD ECHOF FLCL ISM MDAL PMS RSBR RS485 SCRO STO ALIN AMOD APOL AUXV DINL DOL DOT ENCO PLIN PMOD PPOL BPR BLFB BLSTD CLERD LEAC MLX MDIR MMOD MXMD SCLK SED SFTS BFBK BMOD HPO SWD CAS CBR CEN CHLA CTT FSA',
    'conforme PMOD': 'PCTR PMIN PMAX',
    'conforme MMOD': 'FEW',
    'ms / fator': 'CGT',
    'ID + ms': 'CHBT',
    'mapa PDO': 'RPDM TPDM',
    'COB-ID': 'RPDC TPDC',
}
NOTES = {
    'DFC': 'Comando normalizado de -1000 a +1000; não é RPM nem corrente.',
    'EPPR': 'Pulsos por volta (PPR), antes da multiplicação por quadratura; não confundir com contagens/volta.',
    'B25': 'Coeficiente beta do termistor em kelvin; não é a temperatura medida.',
    'CGT': 'Índice 1: tempo em ms. Índice 2: fator multiplicador adimensional; tempo de vida = tempo × fator.',
    'CHBT': 'Inteiro codificado: ID do nó nos bits 16–22 e tempo em ms nos bits 0–15. Ex.: 133072 = nó 2, 2000 ms.',
    'RPDM': 'Mapeamento codificado de objeto PDO: índice, subíndice e comprimento. O inteiro completo não é uma duração ou contagem.',
    'TPDM': 'Mapeamento codificado de objeto PDO: índice, subíndice e comprimento. O inteiro completo não é uma duração ou contagem.',
    'ZSMA': 'Amplitude de calibração Sin/Cos. O manual não define conversão para volts; preservado o coeficiente do controlador.',
    'ZSMC': 'Calibração Sin/Cos: pontos zero e razão. O manual não define conversão física comum; preservados os coeficientes.',
    'FEW': 'Erro de malha: RPM em velocidade, contagens em posição. A unidade depende de MMOD; não alterar a escala do registrador.',
    'CBR': 'Seletor de taxa CAN em kbit/s; o número em Atual é o código, não a taxa.',
    'RSBR': 'Seletor de baudrate em bit/s; o número em Atual é o código, não a taxa.',
    'SCLK': 'Seletor de frequência SSI em kHz; o número em Atual é o código, não a frequência.',
}
for key in 'BADJ BADV HSAT PSA'.split():
    NOTES[key] = 'Ângulo em passos de 1/512 de volta (128 = 90°). Preservado o passo inteiro; não interpretar 128 como 128°. Para BADJ/BADV/HSAT, a volta é elétrica.'
for key in 'PCTR PMIN PMAX'.split():
    NOTES[key] = 'Captura: largura em µs; frequência em Hz; duty cycle em contagens 0–4095 (4095 = 100%); contador em pulsos. Depende do PMOD lido para esta entrada.'

def apply_units(specs):
    units = {key: unit for unit, keys in GROUPS.items() for key in keys.split()}
    for spec in specs:
        key = spec['key']
        if key in units:
            spec['unit'] = units[key]
        if key == 'TID':
            spec.update(scale=10, unit='A')
        if key in NOTES:
            spec['unit_note'] = NOTES[key]
        if key == 'CGT':
            spec['index_units'] = {'1': 'ms', '2': 'fator'}
        if spec['unit'] == 'bruto':
            raise ValueError('Unidade não revisada: ' + key)
    return specs

if __name__ == '__main__':
    path = Path(sys.argv[1])
    data = json.loads(path.read_text())
    apply_units(data['parameters'])
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')
