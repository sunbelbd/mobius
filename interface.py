import ctypes
import numpy.ctypeslib as ctl
from numpy.ctypeslib import ndpointer
import numpy as np
from ctypes import *
import os

lib = ctypes.cdll.LoadLibrary("./mobius.so")
build_mobius_index = lib.build_mobius_index
build_mobius_index.restype = None
build_mobius_index.argtypes = [ctl.ndpointer(np.float64, flags='aligned, c_contiguous'), ctypes.c_int, ctypes.c_int, ctypes.c_double]

build_ipwrap_index32 = lib.build_ipwrap_index32
build_ipwrap_index32.restype = None
build_ipwrap_index32.argtypes = [ctl.ndpointer(np.float32, flags='aligned, c_contiguous'), ctypes.c_int, ctypes.c_int, ctypes.c_double, ctypes.c_int]

class MobiusContext(Structure):
    _fields_=[("graph",c_void_p),
              ("data",c_void_p)]

load_mobius_index = lib.load_mobius_index
load_mobius_index.restype = None
load_mobius_index.argtypes = [ctypes.c_int, ctypes.c_int,POINTER(MobiusContext)]

load_ipwrap_index = lib.load_ipwrap_index
load_ipwrap_index.restype = None
load_ipwrap_index.argtypes = [ctypes.c_int, ctypes.c_int,POINTER(MobiusContext), ctypes.c_int]

load_ipwrap_index_prefix = lib.load_ipwrap_index_prefix
load_ipwrap_index_prefix.restype = None
load_ipwrap_index_prefix.argtypes = [ctypes.c_int, ctypes.c_int, POINTER(MobiusContext), ctypes.c_char_p]

save_ipwrap_index_prefix = lib.save_ipwrap_index_prefix
save_ipwrap_index_prefix.restype = None
save_ipwrap_index_prefix.argtypes = [POINTER(MobiusContext), ctypes.c_char_p]

search_mobius_index = lib.search_mobius_index
search_mobius_index.restype = None
search_mobius_index.argtypes = [ctl.ndpointer(np.float64, flags='aligned, c_contiguous'), ctypes.c_int, ctypes.c_int,ctypes.c_int,POINTER(MobiusContext),ctl.ndpointer(np.uint64, flags='aligned, c_contiguous'),ctl.ndpointer(np.float64, flags='aligned, c_contiguous')]

search_ipwrap_index32 = lib.search_ipwrap_index32
search_ipwrap_index32.restype = None
search_ipwrap_index32.argtypes = [ctl.ndpointer(np.float32, flags='aligned, c_contiguous'), ctypes.c_int, ctypes.c_int,ctypes.c_int,POINTER(MobiusContext),ctl.ndpointer(np.uint64, flags='aligned, c_contiguous'),ctl.ndpointer(np.float64, flags='aligned, c_contiguous')]

init_ipwrap_index32 = lib.init_ipwrap_index32
init_ipwrap_index32.restype = None
init_ipwrap_index32.argtypes = [ctypes.c_int, ctypes.c_int,ctypes.c_double, POINTER(MobiusContext)]

release_context = lib.release_context
release_context.restype = None
release_context.argtypes = [POINTER(MobiusContext)]

insert_ipwrap_index32 = lib.insert_ipwrap_index32
insert_ipwrap_index32.restype = None
insert_ipwrap_index32.argtypes = [ctl.ndpointer(np.float32, flags='aligned, c_contiguous'), ctypes.c_int, ctypes.c_int, ctypes.c_int, POINTER(MobiusContext)]

set_construct_pq_size = lib.set_construct_pq_size
set_construct_pq_size.restype = None
set_construct_pq_size.argtypes = [POINTER(MobiusContext), ctypes.c_int]

class MobiusMIPS(object):
  """
  Maximum Inner Product Search Using SONG.

  Use SONG API.
  """
  def __init__(self, total_num, dim, index_vectors, num_neighbors, search_budget, factor):
    """
    init
    """
    self.total_num = total_num
    self.dim = dim
    self.num_neighbors = num_neighbors
    self.factor = factor * 1.0

    build_mobius_index(index_vectors, total_num, dim, self.factor)
    self.song_context = MobiusContext(0,0)
    load_mobius_index(total_num, dim, ctypes.byref(self.song_context))
    
    self.search_budget = search_budget
    self.return_k = num_neighbors
  

  def search(self, query):
    """
    search
    """

    ret_id = np.zeros(self.return_k, dtype=np.uint64)
    ret_score = np.zeros(self.return_k, dtype=np.float64)

    search_mobius_index(query, self.dim, self.search_budget, self.return_k,ctypes.byref(self.song_context),ret_id,ret_score)

    ret_id = ret_id.astype(np.int32)
    return ret_score, ret_id

class MobiusIPWrap(object):
  """
  Maximum Inner Product Search Using SONG.

  Use SONG API.
  """
  def deprecated__init__(self, total_num, dim, index_vectors, num_neighbors, search_budget, max_ip_norm, suffix):
    """
    init
    """
    self.total_num = total_num
    self.dim = dim
    self.num_neighbors = num_neighbors
    self.max_ip_norm = max_ip_norm
    self.suffix = suffix

    build_ipwrap_index32(index_vectors, total_num, dim, max_ip_norm, suffix)
    self.song_context = MobiusContext(0,0)
    load_ipwrap_index(total_num, dim, ctypes.byref(self.song_context), suffix)
    
    self.search_budget = search_budget
    self.return_k = num_neighbors
  
  def __init__(self, total_num, dim, num_neighbors, search_budget, max_ip_norm,prefix = ''):
    """
    init
    """
    self.total_num = total_num
    self.dim = dim
    self.num_neighbors = num_neighbors
    self.max_ip_norm = max_ip_norm
    self.suffix = 0

    if prefix != '':
      self.song_context = MobiusIPWrap.load(total_num,dim,prefix)
    else:
      self.song_context = MobiusContext(0,0)
      init_ipwrap_index32(self.total_num,self.dim,self.max_ip_norm,ctypes.byref(self.song_context))
    
    self.search_budget = search_budget
    self.return_k = num_neighbors
  
  def insert(self, vectors, row, dim, offset):
    insert_ipwrap_index32(vectors, row, dim, offset, ctypes.byref(self.song_context))

  def set_construct_budget(self, size):
    set_construct_pq_size(ctypes.byref(self.song_context),size)
  
  def search(self, query):
    """
    search
    """

    ret_id = np.zeros(self.return_k, dtype=np.uint64)
    ret_score = np.zeros(self.return_k, dtype=np.float64)

    search_ipwrap_index32(query, self.dim, self.search_budget, self.return_k,ctypes.byref(self.song_context),ret_id,ret_score)

    ret_id = ret_id.astype(np.int32)
    return ret_score, ret_id

  def release(self):
    release_context(ctypes.byref(self.song_context))

  def save(self,prefix):
    save_ipwrap_index_prefix(ctypes.byref(self.song_context),prefix)

  @staticmethod
  def load(row,dim,prefix):
    context = MobiusContext(0,0)
    load_ipwrap_index_prefix(row,dim,ctypes.byref(context),prefix)
    return context

