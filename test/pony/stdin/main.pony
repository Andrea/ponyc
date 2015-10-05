class Notify is StdinNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref apply(data: Array[U8] iso) =>
    let data' = consume val data

    for c in data'.values() do
      _env.out.write(c.string(FormatHex))
    end

    _env.out.write("\n")

actor Main
  new create(env: Env) =>
    env.input(Notify(env))
