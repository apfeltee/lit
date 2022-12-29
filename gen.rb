#!/usr/bin/ruby

require "ostruct"
require "optparse"
require "fileutils"
require "yaml"

def do_gsub(file, mapping)
  # let's be dumb.
  changecnt = 0
  data = File.read(file)
  mapping.each do |old, new|
    rx = /\b#{old}\b/
    cnt = data.scan(rx).length
    changecnt += cnt
    $stderr.printf("gsub: %d matches for %p\n", cnt, rx)
    data.gsub!(rx, new)
  end
  if changecnt > 0 then
    File.write(file, data)
  else
    $stderr.printf("no matches whatsoever. nothing written.\n")
  end
end

def fail_if_not_string(what, key=nil)
  if !what.is_a?(String) then
    if key == nil then
      $stderr.printf("error: key %p is not a string\n", what)
    else
      $stderr.printf("error: value %p (of key %p) is not a string\n", what, key)
    end
    return 1
  end
  return 0
end

def fail_if_not_sym(what, key=nil)
  if fail_if_not_string(what, key) == 0 then
    if !what.match(/^\w+$/) then
      if key == nil then
        $stderr.printf("error: key %p does not look like a full word", what)
      else
        $stderr.printf("error: value %p (of key %p) does not look like a full word\n", what, key)
      end
      return 1
    end
  end
  return 0
end


def runcproto(files, opts)
  cmd = ["cproto", "-si", "-P", '/* int */ f // (a, b)', *files]
  IO.popen(cmd, "rb") do |io|
    nlines = {}
    raw = io.read
    lines = raw.split(/\n/)
    lines.each do |s|
      s.strip!
      rawline = s.clone
      rawline = rawline.gsub(Regexp.new('/\*\s*(.*)\s*\*/'), '\1').gsub(Regexp.new('\s*//\s*'), "")
      rawline = rawline.gsub(/\s+/, " ").strip
      if opts.onlystatics && (not s.include?("static")) then
        next
      end
      if !opts.alsoinline && (s.include?("inline")) then
        next
      end
      nlines[rawline] = s
    end
    rt = []
    return nlines.map{|raw, s|
      [raw, s.gsub(/\/\*(\*(?!\/)|[^*])*\*\//, "").gsub(/\/\/.*$/, "").strip]
    }.reject{|raw, s| s.empty? }
  end
end

def handlesym(name)

end

def run_generate
  ofile = nil
  ofh = $stdout
  opts = OpenStruct.new({
    onlystatics: false,
    alsoinline: false,
    prefix: "",
  })
  OptionParser.new{|prs|
    prs.on("-o<file>", "write output to <file> instead of stdout"){|v|
      ofile = File.open(v, "wb")
      ofh = ofile
    }
    prs.on("-p<str>", "set prefix to <str>"){|v|
      opts.prefix = v
    }
    prs.on("-s", "--static", "emit only static functions"){
      opts.onlystatics = true
    }
    prs.on("-i", "--inline", "also emit inline functions"){
      opts.alsoinline = true
    }
  }.parse!
  begin
    files = ARGV
    runcproto(files, opts).each do |srcfunc, old|
      new = sprintf("%s%s", opts.prefix, old.gsub(/internal_/, ""))
      if ofile != nil then
        $stderr.printf("from %s:\n", srcfunc)
        $stderr.printf(" %p -> %p\n", old, new)
      end
      ofh.printf("  # %s\n", srcfunc)
      ofh.printf("  %p: %p\n", old, new)
      ofh.printf("\n")
    end
  ensure
    if ofile != nil then
      ofile.close
    end
  end
  exit(0)
end

def run_replace
  OptionParser.new{|prs|
    prs.on("-h", "--help"){
      [
        "reads a symbol table in YAML format, and renames the given symbols accordingly.",
        "",
        "the symbol table must yield a hash with string keys and string values. for example:",
        "# syms.yml",
        "   \"my_silly_symbol\": \"new_silly_name\"",
        "   \"another_sym\": \"another_new_name\"",
        "# ....",
        "",
        "the resulting symbols are then fed to 'gsub'.",
        "make sure it is in your PATH.",
      ].each{|l| puts(l)}
      exit(0)
    }
  }.parse!
  names = {}
  failcnt = 0
  symfile = ARGV.shift
  if (symfile == nil) || !File.file?(symfile) then
    failcnt += 1
    $stderr.printf("error: need a symbol table that exists\n")
  else
    if !symfile.match(/\.ya?ml$/) then
      failcnt += 1
      $stderr.printf("first file argument is expected to be a YAML file\n")
    end
  end
  thefiles = ARGV
  if thefiles.empty? then
    failcnt += 1
    $stderr.printf("error: no files specified\n")
  end
  thefiles.each do |file|
    if !File.file?(file) then
      failcnt += 1
      $stderr.printf("error: file %p does not exist\n", file)
    end
  end
  begin
    uniqnames = []
    rawnames = YAML.load(File.read("syms.yml"))
    if !rawnames.is_a?(Hash) then
      failcnt += 1
      $stderr.printf("error: reading from name table did not return a hash!\n")
    end
    rawnames.each do |k, v|
      failcnt += fail_if_not_sym(k)
      failcnt += fail_if_not_sym(v, k)
      if uniqnames.include?(v) then
        failcnt += 1
        $stderr.printf("error: name %p (from %p) conflicts with a previous one!\n", v, k)
      else
        names[k] = v
        uniqnames.push(v)
      end
    end
  rescue => ex
    failcnt += 1
    $stderr.printf("exception while reading symbols: (%s) %s\n", ex.class.name, ex.message)
  end
  if failcnt != 0 then
    $stderr.printf("error: too many errors. cannot continue.\n")
    exit(1)
  end
  thefiles.each do |thefile|
    base = File.basename(thefile)
    ext = File.extname(base)
    stem = File.basename(base, ext)
    backuploc = File.join("_backup", sprintf("%s.backup_%s.%s", stem, Process.pid, ext[1 .. -1]))
    if not File.directory?(bpdir = File.dirname(backuploc)) then
      FileUtils.mkdir_p(bpdir)
    end
    FileUtils.copy(thefile, backuploc)
    $stderr.printf("backup is at %p\n", backuploc)
    ## don't actually need gsub here, methinks
    #mapping = names.map{|old, new| sprintf("%s=%s", old, new)}
    #cmd = ["gsub", "-r", "-f", thefile, "-b", *mapping]
    #system(*cmd)
    do_gsub(thefile, names)
  end
  exit(0)
end

begin
  verb = ARGV.shift
  if verb then
    verb = verb.downcase
    if verb.match?(/^g(en(erate)?)?/) then
      run_generate
    elsif verb.match?(/^r(e(place)?)?/) then
      run_replace
    else
      $stderr.printf("unrecognized verb %p\n", verb)
      exit(1)
    end
  else
    $stderr.printf("specify 'generate' or 'replace'\n")
    exit(1)
  end
end

