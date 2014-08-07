# Groonga highlight function

## Install

Install libgroonga-dev.

Build this function.

    % sh autogen.sh
    % ./configure
    % make
    % sudo make install

## Usage

Register `function/highlight`:

    % groonga DB
    > register function/highlight

Now, you can use `highlight_full` function

```
select Entries --output_columns 'highlight_full(body, 8192, "NormalizerAuto", \
  "Groonga", "<span class=\\"keyword1\\">", "</span>", \
  "mroonga", "<span class=\\"keyword2\\">", "</span>")' --command_version 2
[
  [
    0,
    0.0,
    0.0
  ],
  [
    [
      [
        1
      ],
      [
        [
          "highlight_full",
          "Object"
        ]
      ],
      [
        "This is test strings. <span class=\"keyword2\">Mroonga</span> is a ＭｙＳＱＬ storage engine based on <span class=\"keyword1\">Ｇｒｏｏｎｇａ</span>. Rroonga is a Ruby binding of <span class=\"keyword1\">Groonga</span>."
      ]
    ]
  ]
]
```

## Author

Naoya Murakami naoya@createfield.com

## License

LGPL 2.1. See COPYING-LGPL-2.1 for details.
