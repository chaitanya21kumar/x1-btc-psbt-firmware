# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: perform_attestation.proto
"""Generated protocol buffer code."""
from google.protobuf.internal import builder as _builder
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x19perform_attestation.proto\x12\x14shiftcrypto.x1-btc-psbt-firmware\".\n\x19PerformAttestationRequest\x12\x11\n\tchallenge\x18\x01 \x01(\x0c\"\x9e\x01\n\x1aPerformAttestationResponse\x12\x17\n\x0f\x62ootloader_hash\x18\x01 \x01(\x0c\x12\x15\n\rdevice_pubkey\x18\x02 \x01(\x0c\x12\x13\n\x0b\x63\x65rtificate\x18\x03 \x01(\x0c\x12\x1e\n\x16root_pubkey_identifier\x18\x04 \x01(\x0c\x12\x1b\n\x13\x63hallenge_signature\x18\x05 \x01(\x0c\x62\x06proto3')

_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, globals())
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'perform_attestation_pb2', globals())
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  _PERFORMATTESTATIONREQUEST._serialized_start=51
  _PERFORMATTESTATIONREQUEST._serialized_end=97
  _PERFORMATTESTATIONRESPONSE._serialized_start=100
  _PERFORMATTESTATIONRESPONSE._serialized_end=258
# @@protoc_insertion_point(module_scope)
